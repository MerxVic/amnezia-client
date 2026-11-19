#include "ipSplitTunnelingUiController.h"

#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QSettings>
#include <memory>

#include "systemController.h"
#include "core/utils/errorCodes.h"
#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"

IpSplitTunnelingUiController::IpSplitTunnelingUiController(IpSplitTunnelingController* ipSplitTunnelingController,
                                                          IpSplitTunnelingModel* ipSplitTunnelingModel,
                                                          ConnectionController* connectionController,
                                                          QObject *parent)
    : QObject(parent),
      m_ipSplitTunnelingController(ipSplitTunnelingController),
      m_ipSplitTunnelingModel(ipSplitTunnelingModel),
      m_connectionController(connectionController)
{
    m_ipSplitTunnelingModel->updateModel(m_ipSplitTunnelingController->getCurrentSites());
    m_managedPolicyUrl = QSettings().value(QStringLiteral("VDS/managedBypassUrl")).toString();
    m_policyTimer.setInterval(60 * 60 * 1000);
    connect(&m_policyTimer, &QTimer::timeout, this, [this]() {
        downloadManagedBypassPolicy(m_managedPolicyUrl);
    });
    connect(m_connectionController, &ConnectionController::connectionStateChanged,
            this, [this](Vpn::ConnectionState state) {
                if (state == Vpn::ConnectionState::Disconnected) applyPendingPolicy();
            });
    if (!m_managedPolicyUrl.isEmpty()) {
        m_policyTimer.start();
        QTimer::singleShot(0, this, [this]() {
            downloadManagedBypassPolicy(m_managedPolicyUrl);
        });
    }
}

void IpSplitTunnelingUiController::addSite(QString hostname)
{
    if (m_ipSplitTunnelingController->addSite(hostname)) {
        updateModel();
        emit finished(tr("New site added: %1").arg(hostname));
    }
}

void IpSplitTunnelingUiController::removeSite(int index)
{
    auto modelIndex = m_ipSplitTunnelingModel->index(index);
    auto hostname = m_ipSplitTunnelingModel->data(modelIndex, IpSplitTunnelingModel::Roles::UrlRole).toString();
    if (m_ipSplitTunnelingController->removeSite(hostname)) {
        updateModel();
        emit finished(tr("Site removed: %1").arg(hostname));
    }
}

void IpSplitTunnelingUiController::removeSites()
{
    const bool preserveManaged = m_ipSplitTunnelingController->getRouteMode() == amnezia::RouteMode::VpnAllExceptSites &&
                                 m_ipSplitTunnelingController->hasManagedBypassSites();
    m_ipSplitTunnelingController->removeSites();
    updateModel();
    emit finished(preserveManaged ? tr("Manual site list cleared") : tr("Site list cleared!"));
}

void IpSplitTunnelingUiController::importSites(const QString &fileName, bool replaceExisting)
{
    QByteArray jsonData;
    if (!SystemController::readFile(fileName, jsonData)) {
        emit errorOccurred(tr("Can't open file: %1").arg(fileName));
        return;
    }

    QString errorMessage;
    if (m_ipSplitTunnelingController->importSitesFromJson(jsonData, replaceExisting, errorMessage)) {
        updateModel();
        emit finished(tr("Import completed"));
    } else {
        emit errorOccurred(errorMessage);
    }
}

void IpSplitTunnelingUiController::importManagedBypassPolicy(const QString &fileName)
{
    QByteArray jsonData;
    if (!SystemController::readFile(fileName, jsonData)) {
        emit errorOccurred(tr("Can't open file: %1").arg(fileName));
        return;
    }
    QString errorMessage;
    if (m_ipSplitTunnelingController->importManagedBypassPolicy(jsonData, errorMessage)) {
        m_managedPolicyWarning = errorMessage;
        emit managedPolicyWarningChanged();
        emit managedPolicyChanged();
        updateModel();
        emit routeModeChanged();
        emit isSplitTunnelingEnabledChanged();
        emit finished(tr("Direct-route list imported") +
                      (errorMessage.isEmpty() ? QString() : QStringLiteral(". ") + errorMessage));
    } else {
        emit errorOccurred(errorMessage);
    }
}

void IpSplitTunnelingUiController::downloadManagedBypassPolicy(const QString &urlText)
{
    const QUrl url(urlText);
    if (m_policyDownloadActive || !url.isValid() || url.scheme() != QLatin1String("https") ||
        url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() ||
        url.hasFragment() || url.path() != QLatin1String("/downloads/ru-bypass-ip-list.json")) {
        emit errorOccurred(tr("Enter a valid HTTPS bypass-list URL"));
        return;
    }
    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    m_policyDownloadActive = true;
    QNetworkReply *reply = m_policyNetwork.get(request);
    auto content = std::make_shared<QByteArray>();
    auto tooLarge = std::make_shared<bool>(false);
    connect(reply, &QNetworkReply::readyRead, this, [reply, content, tooLarge]() {
        if (*tooLarge) return;
        content->append(reply->readAll());
        if (content->size() > 256 * 1024) {
            *tooLarge = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, content, tooLarge]() {
        m_policyDownloadActive = false;
        if (!*tooLarge) content->append(reply->readAll());
        const bool received = !*tooLarge && reply->error() == QNetworkReply::NoError &&
                              reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
        const QUrl responseUrl = reply->url();
        reply->deleteLater();
        if (!received) {
            emit errorOccurred(tr("Could not download the bypass list"));
            return;
        }
        m_pendingPolicy = *content;
        m_pendingPolicyUrl = responseUrl;
        if (m_connectionController->connectionState() == Vpn::ConnectionState::Disconnected) {
            applyPendingPolicy();
        } else {
            emit finished(tr("Direct-route list will update after VPN disconnects"));
        }
    });
}

void IpSplitTunnelingUiController::applyPendingPolicy()
{
    if (m_pendingPolicy.isEmpty()) return;
    QString errorMessage;
    if (!m_ipSplitTunnelingController->importManagedBypassPolicy(m_pendingPolicy, errorMessage)) {
        m_pendingPolicy.clear();
        emit errorOccurred(errorMessage);
        return;
    }
    m_pendingPolicy.clear();
    m_managedPolicyWarning = errorMessage;
    emit managedPolicyWarningChanged();
    emit managedPolicyChanged();
    const QString acceptedUrl = m_pendingPolicyUrl.toString();
    if (m_managedPolicyUrl != acceptedUrl) {
        m_managedPolicyUrl = acceptedUrl;
        QSettings().setValue(QStringLiteral("VDS/managedBypassUrl"), acceptedUrl);
        emit managedPolicyUrlChanged();
    }
    m_policyTimer.start();
    updateModel();
    emit routeModeChanged();
    emit isSplitTunnelingEnabledChanged();
    emit finished(tr("Direct-route list updated") +
                  (errorMessage.isEmpty() ? QString() : QStringLiteral(". ") + errorMessage));
}

void IpSplitTunnelingUiController::exportSites(const QString &fileName)
{
    QByteArray jsonData = m_ipSplitTunnelingController->exportSitesToJson();
    if (!SystemController::saveFile(fileName, jsonData)) {
        qInfo() << "IpSplitTunnelingUiController::exportSites: save or share was cancelled or failed";
        return;
    }
    emit finished(tr("Export completed"));
}

void IpSplitTunnelingUiController::toggleSplitTunneling(bool enabled)
{
    m_ipSplitTunnelingController->toggleSplitTunneling(enabled);
    emit isSplitTunnelingEnabledChanged();
}

void IpSplitTunnelingUiController::setRouteMode(int routeMode)
{
    m_ipSplitTunnelingController->setRouteMode(static_cast<amnezia::RouteMode>(routeMode));
    emit routeModeChanged();
}

int IpSplitTunnelingUiController::getRouteMode() const
{
    return static_cast<int>(m_ipSplitTunnelingController->getRouteMode());
}

bool IpSplitTunnelingUiController::isSplitTunnelingEnabled() const
{
    return m_ipSplitTunnelingController->isSplitTunnelingEnabled();
}

bool IpSplitTunnelingUiController::hasManagedBypassSites() const
{
    return m_ipSplitTunnelingController->hasManagedBypassSites();
}

bool IpSplitTunnelingUiController::isManagedBypassSite(const QString &hostname) const
{
    return m_ipSplitTunnelingController->isManagedBypassSite(hostname);
}

void IpSplitTunnelingUiController::updateModel()
{
    m_ipSplitTunnelingModel->updateModel(m_ipSplitTunnelingController->getCurrentSites());
}
