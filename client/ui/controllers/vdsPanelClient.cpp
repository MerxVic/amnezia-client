#include "vdsPanelClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
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
            fail(tr("This account is not assigned a client VPN profile"));
            return;
        }
        emit roleReady(QStringLiteral("client"));
        fetchProfile();
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
    });
}
