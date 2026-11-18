#include "ipSplitTunnelingUiController.h"

#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <memory>

#include "systemController.h"
#include "core/utils/errorCodes.h"
#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"

IpSplitTunnelingUiController::IpSplitTunnelingUiController(IpSplitTunnelingController* ipSplitTunnelingController,
                                                          IpSplitTunnelingModel* ipSplitTunnelingModel, QObject *parent)
    : QObject(parent),
      m_ipSplitTunnelingController(ipSplitTunnelingController),
      m_ipSplitTunnelingModel(ipSplitTunnelingModel)
{
    m_ipSplitTunnelingModel->updateModel(m_ipSplitTunnelingController->getCurrentSites());
}

void IpSplitTunnelingUiController::addSite(QString hostname)
{
    if (m_ipSplitTunnelingController->addSite(hostname)) {
        emit finished(tr("New site added: %1").arg(hostname));
    }
}

void IpSplitTunnelingUiController::removeSite(int index)
{
    auto modelIndex = m_ipSplitTunnelingModel->index(index);
    auto hostname = m_ipSplitTunnelingModel->data(modelIndex, IpSplitTunnelingModel::Roles::UrlRole).toString();
    if (m_ipSplitTunnelingController->removeSite(hostname)) {
        emit finished(tr("Site removed: %1").arg(hostname));
    }
}

void IpSplitTunnelingUiController::removeSites()
{
    m_ipSplitTunnelingController->removeSites();
    emit finished(tr("Site list cleared!"));
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
        updateModel();
        emit routeModeChanged();
        emit isSplitTunnelingEnabledChanged();
        emit finished(tr("Direct-route list imported"));
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
        reply->deleteLater();
        if (!received) {
            emit errorOccurred(tr("Could not download the bypass list"));
            return;
        }
        QString errorMessage;
        if (!m_ipSplitTunnelingController->importManagedBypassPolicy(*content, errorMessage)) {
            emit errorOccurred(errorMessage);
            return;
        }
        updateModel();
        emit routeModeChanged();
        emit isSplitTunnelingEnabledChanged();
        emit finished(tr("Direct-route list updated"));
    });
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

void IpSplitTunnelingUiController::updateModel()
{
    m_ipSplitTunnelingModel->updateModel(m_ipSplitTunnelingController->getCurrentSites());
}
