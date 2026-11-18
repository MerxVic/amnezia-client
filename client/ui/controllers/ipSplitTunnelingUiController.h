#ifndef IPSPLITTUNNELINGUICONTROLLER_H
#define IPSPLITTUNNELINGUICONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QUrl>

#include "core/controllers/connectionController.h"
#include "core/controllers/ipSplitTunnelingController.h"
#include "ui/models/ipSplitTunnelingModel.h"

class IpSplitTunnelingUiController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int routeMode READ getRouteMode WRITE setRouteMode NOTIFY routeModeChanged)
    Q_PROPERTY(bool isSplitTunnelingEnabled READ isSplitTunnelingEnabled NOTIFY isSplitTunnelingEnabledChanged)
    Q_PROPERTY(QString managedPolicyUrl READ managedPolicyUrl NOTIFY managedPolicyUrlChanged)
    Q_PROPERTY(QString managedPolicyWarning READ managedPolicyWarning NOTIFY managedPolicyWarningChanged)

public:
    explicit IpSplitTunnelingUiController(IpSplitTunnelingController* ipSplitTunnelingController,
                                         IpSplitTunnelingModel* ipSplitTunnelingModel,
                                         ConnectionController* connectionController,
                                         QObject *parent = nullptr);
    QString managedPolicyUrl() const { return m_managedPolicyUrl; }
    QString managedPolicyWarning() const { return m_managedPolicyWarning; }

public slots:
    void addSite(QString hostname);
    void removeSite(int index);
    void removeSites();
    void importSites(const QString &fileName, bool replaceExisting);
    void importManagedBypassPolicy(const QString &fileName);
    void downloadManagedBypassPolicy(const QString &url);
    void exportSites(const QString &fileName);
    void toggleSplitTunneling(bool enabled);
    void setRouteMode(int routeMode);

    int getRouteMode() const;
    bool isSplitTunnelingEnabled() const;

    void updateModel();

signals:
    void routeModeChanged();
    void isSplitTunnelingEnabledChanged();
    void managedPolicyUrlChanged();
    void managedPolicyWarningChanged();
    void errorOccurred(const QString &errorMessage);
    void finished(const QString &message);
    void saveFile(const QString &fileName, const QString &data);

private:
    IpSplitTunnelingController* m_ipSplitTunnelingController;
    IpSplitTunnelingModel* m_ipSplitTunnelingModel;
    ConnectionController* m_connectionController;
    QNetworkAccessManager m_policyNetwork;
    QTimer m_policyTimer;
    QString m_managedPolicyUrl;
    QString m_managedPolicyWarning;
    QUrl m_pendingPolicyUrl;
    QByteArray m_pendingPolicy;
    bool m_policyDownloadActive = false;
    void applyPendingPolicy();
};

#endif // IPSPLITTUNNELINGUICONTROLLER_H
