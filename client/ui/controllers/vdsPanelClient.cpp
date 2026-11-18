#include "vdsPanelClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrlQuery>

namespace {
QNetworkRequest panelRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    return request;
}

bool successful(const QNetworkReply *reply, int status)
{
    return reply->error() == QNetworkReply::NoError &&
           reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == status;
}
}

VdsPanelClient::VdsPanelClient(QObject *parent)
    : QObject(parent)
{
    m_network.setCookieJar(new QNetworkCookieJar(&m_network));
    const QString lastRole = QSettings().value(QStringLiteral("VDS/lastPanelRole")).toString();
    if (lastRole == QLatin1String("client") || lastRole == QLatin1String("admin")) {
        m_role = lastRole;
    }
    m_statusTimer.setInterval(60000);
    connect(&m_statusTimer, &QTimer::timeout, this, &VdsPanelClient::fetchStatus);
}

QUrl VdsPanelClient::endpoint(const QString &path) const
{
    QUrl url = m_baseUrl;
    url.setPath(path);
    return url;
}

void VdsPanelClient::fail(const QString &message)
{
    m_busy = false;
    emit errorOccurred(message);
}

void VdsPanelClient::fetchOwnProfile(const QString &panelUrl, const QString &username,
                                     const QString &password)
{
    const QUrl url(panelUrl.trimmed());
    if (m_busy || !url.isValid() || url.scheme() != QLatin1String("https") ||
        url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() ||
        url.hasFragment() || (url.path() != QString() && url.path() != QLatin1String("/")) ||
        username.isEmpty() || password.isEmpty()) {
        emit errorOccurred(tr("Enter a valid HTTPS panel URL and account credentials"));
        return;
    }
    m_busy = true;
    if (m_signedIn) {
        m_signedIn = false;
        emit signedInChanged();
    }
    m_statusTimer.stop();
    m_statusText.clear();
    emit statusChanged();
    m_baseUrl = url;
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("username"), username);
    form.addQueryItem(QStringLiteral("password"), password);
    QNetworkRequest request = panelRequest(endpoint(QStringLiteral("/login")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    QNetworkReply *reply = m_network.post(request, form.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const bool ok = successful(reply, 303);
        reply->deleteLater();
        if (!ok) {
            fail(tr("Could not sign in to the VPN panel"));
            return;
        }
        fetchIdentity();
    });
}

void VdsPanelClient::fetchIdentity()
{
    QNetworkReply *reply = m_network.get(panelRequest(endpoint(QStringLiteral("/api/client/me"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const bool ok = successful(reply, 200);
        const QByteArray body = reply->read(4096);
        reply->deleteLater();
        const QJsonDocument document = QJsonDocument::fromJson(body);
        if (!ok || !document.isObject() ||
            document.object().value("role").toString() != QLatin1String("client")) {
            fetchAdminIdentity();
            return;
        }
        m_role = QStringLiteral("client");
        QSettings().setValue(QStringLiteral("VDS/lastPanelRole"), m_role);
        emit roleChanged();
        emit roleReady(QStringLiteral("client"));
        m_signedIn = true;
        emit signedInChanged();
        m_statusTimer.start();
        fetchProfile();
    });
}

void VdsPanelClient::fetchAdminIdentity()
{
    QNetworkReply *reply = m_network.get(panelRequest(endpoint(QStringLiteral("/api/config"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const bool ok = successful(reply, 200);
        const QByteArray body = reply->read(4097);
        reply->deleteLater();
        const QJsonDocument document = QJsonDocument::fromJson(body);
        const QString role = document.isObject() ? document.object().value("role").toString() : QString();
        if (!ok || body.size() > 4096 || (role != QLatin1String("admin") &&
                                           role != QLatin1String("owner"))) {
            fail(tr("This account has no VPN panel access"));
            return;
        }
        m_role = QStringLiteral("admin");
        QSettings().setValue(QStringLiteral("VDS/lastPanelRole"), m_role);
        m_busy = false;
        m_signedIn = true;
        emit roleChanged();
        emit signedInChanged();
        emit roleReady(m_role);
    });
}

void VdsPanelClient::fetchProfile()
{
    QNetworkReply *reply = m_network.get(panelRequest(endpoint(QStringLiteral("/api/client/profile"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const bool ok = successful(reply, 200);
        const QByteArray body = reply->read(256 * 1024 + 1);
        reply->deleteLater();
        if (!ok || body.isEmpty() || body.size() > 256 * 1024) {
            fail(tr("Could not download this device's VPN profile"));
            return;
        }
        m_busy = false;
        emit profileReady(QString::fromUtf8(body));
        fetchStatus();
    });
}

void VdsPanelClient::fetchStatus()
{
    if (!m_signedIn || m_statusActive) return;
    m_statusActive = true;
    QNetworkReply *reply = m_network.get(panelRequest(endpoint(QStringLiteral("/api/client/status"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_statusActive = false;
        const bool ok = successful(reply, 200);
        const QByteArray body = reply->read(8193);
        reply->deleteLater();
        const QJsonDocument document = QJsonDocument::fromJson(body);
        if (!ok || body.size() > 8192 || !document.isObject()) {
            m_statusText = tr("Server status unavailable");
            emit statusChanged();
            return;
        }
        const QJsonObject status = document.object();
        if (status.value("stale").toBool(true)) {
            m_statusText = tr("No recent server status");
        } else {
            const QJsonObject awg = status.value("awg").toObject();
            const QJsonObject vless = status.value("vless").toObject();
            int awgReady = 0;
            int vlessReady = 0;
            for (const QString &name : {QStringLiteral("aeza"), QStringLiteral("lands"),
                                        QStringLiteral("gefast")}) {
                awgReady += awg.value(name).toBool() ? 1 : 0;
                vlessReady += vless.value(name).toBool() ? 1 : 0;
            }
            m_statusText = tr("AWG: %1/3, VLESS: %2/3").arg(awgReady).arg(vlessReady);
        }
        emit statusChanged();
    });
}
