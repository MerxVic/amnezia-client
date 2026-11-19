#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>
#include <QTest>

#include "utils/testCoreController.h"
#include "core/models/serverDescription.h"
#include "core/controllers/ipSplitTunnelingController.h"
#include "core/repositories/secureAppSettingsRepository.h"
#include "secureQSettings.h"
#include "vpnConnection.h"

using namespace amnezia;

class TestUiIpModelAndController : public QObject
{
    Q_OBJECT

private:
    TestCoreController *m_coreController;
    SecureQSettings *m_settings;

    QString normalizeHostname(const QString &hostname) const
    {
        QString normalized = hostname;
        normalized.replace("https://", "");
        normalized.replace("http://", "");
        normalized.replace("ftp://", "");
        normalized = normalized.split("/", Qt::SkipEmptyParts).first();
        return normalized;
    }

private slots:
    void initTestCase()
    {
        QString testOrg = "AmneziaVPN-Test-" + QUuid::createUuid().toString();
        m_settings = new SecureQSettings(testOrg, "amnezia-client", nullptr, false);

        auto vpnConnection = QSharedPointer<VpnConnection>::create(nullptr, nullptr);

        m_coreController = new TestCoreController(vpnConnection, m_settings, nullptr, this);
    }

    void cleanupTestCase()
    {
        m_settings->clearSettings();
        delete m_coreController;
        delete m_settings;
    }

    void init()
    {
        m_settings->clearSettings();
        if (m_coreController->m_serversModel) {
            m_coreController->m_serversModel->updateModel(QVector<ServerDescription>(), QString{});
        }
    }

    void testRolesAndSignals()
    {
        QSignalSpy finishedSpy(m_coreController->m_ipSplitTunnelingUiController, &IpSplitTunnelingUiController::finished);
        QSignalSpy errorOccurredSpy(m_coreController->m_ipSplitTunnelingUiController, &IpSplitTunnelingUiController::errorOccurred);
        QSignalSpy isSplitTunnelingChangedSpy(m_coreController->m_ipSplitTunnelingUiController, &IpSplitTunnelingUiController::isSplitTunnelingEnabledChanged);

        m_coreController->m_ipSplitTunnelingUiController->toggleSplitTunneling(true);
        QVERIFY2(isSplitTunnelingChangedSpy.count() == 1, "isSplitTunnelingChangedSpy signal should be emitted");
        QVERIFY2(m_coreController->m_ipSplitTunnelingUiController->isSplitTunnelingEnabled() == true, "ipSplitTunneling should be enabled");

        m_coreController->m_ipSplitTunnelingUiController->toggleSplitTunneling(false);
        QVERIFY2(isSplitTunnelingChangedSpy.count() == 2, "isSplitTunnelingChangedSpy signal should be emitted 2nd time");
        QVERIFY2(m_coreController->m_ipSplitTunnelingUiController->isSplitTunnelingEnabled() == false, "ipSplitTunneling should be disabled");

        QString site = "2ip.io";

        m_coreController->m_ipSplitTunnelingUiController->addSite(site);
        m_coreController->m_ipSplitTunnelingUiController->addSite("whatismyipaddress.com");
        m_coreController->m_ipSplitTunnelingUiController->updateModel();
        QVERIFY2(finishedSpy.count() == 2, "finished signal should be emitted 2 times");
        QVERIFY2(m_coreController->m_ipSplitTunnelingModel->rowCount() == 2, "IpSplitTunnelingModel should have 2 rows");

        QModelIndex siteModelIndex = m_coreController->m_ipSplitTunnelingModel->index(0, 0);
        QVERIFY2(siteModelIndex.isValid(), "Ip model index should be valid");

        auto siteUrl = m_coreController->m_ipSplitTunnelingModel->data(siteModelIndex, IpSplitTunnelingModel::UrlRole);
        QCOMPARE(siteUrl, normalizeHostname(site));

        auto siteIp = m_coreController->m_ipSplitTunnelingModel->data(siteModelIndex, IpSplitTunnelingModel::IpRole);
        QVERIFY2(siteIp.isNull() == false, "Ip should not be empty");

        m_coreController->m_ipSplitTunnelingUiController->removeSite(0);
        m_coreController->m_ipSplitTunnelingUiController->updateModel();
        QVERIFY2(finishedSpy.count() == 3, "finished signal should be emitted");
        QVERIFY2(m_coreController->m_ipSplitTunnelingModel->rowCount() == 1, "IpSplitTunnelingModel should have 1 row");

        QTemporaryDir tempDir;
        QVERIFY2(tempDir.isValid(), "Temporary directory should be created");

        const QString sitesFile = tempDir.filePath(QStringLiteral("sites.json"));
        {
            QFile file(sitesFile);
            QVERIFY2(file.open(QIODevice::WriteOnly), "Sites import file should be writable");
            file.write(R"([
                {"hostname": "example.com", "ip": "93.184.216.34"},
                {"hostname": "test.org", "ip": "1.2.3.4"}
            ])");
        }

        m_coreController->m_ipSplitTunnelingUiController->importSites(sitesFile, true);
        m_coreController->m_ipSplitTunnelingUiController->updateModel();
        QVERIFY2(errorOccurredSpy.count() == 0, "errorOccurred signal should not be emitted");
        QVERIFY2(finishedSpy.count() == 4, "finished signal should be emitted");
        QVERIFY2(m_coreController->m_ipSplitTunnelingModel->rowCount() > 1, "IpSplitTunnelingModel should have more than 1 row");

        const QString exportFile = tempDir.filePath(QStringLiteral("test_ips_export.json"));
        m_coreController->m_ipSplitTunnelingUiController->exportSites(exportFile);
        QVERIFY2(finishedSpy.count() == 5, "finished signal should be emitted");

        m_coreController->m_ipSplitTunnelingUiController->removeSites();
        m_coreController->m_ipSplitTunnelingUiController->updateModel();
        QVERIFY2(finishedSpy.count() == 6, "finished signal should be emitted");
        QVERIFY2(m_coreController->m_ipSplitTunnelingModel->rowCount() == 0, "IpSplitTunnelingModel should have 0 rows");
    }

    void testManagedBypassPreservesManualSites()
    {
        SecureQSettings settings("AmneziaVPN-Bypass-Test-" + QUuid::createUuid().toString(),
                                 "amnezia-client", nullptr, false);
        SecureAppSettingsRepository repository(&settings);
        repository.addVpnSite(RouteMode::VpnAllExceptSites, "192.0.2.0/24");

        {
            IpSplitTunnelingController controller(&repository);
            QString error;
            QVERIFY(controller.importManagedBypassPolicy(
                R"([{"hostname":"198.51.100.0/24","ip":""}])", error));
            QVERIFY(error.isEmpty());
            QVERIFY(controller.addSite("203.0.113.0/25"));
            QVERIFY(controller.importManagedBypassPolicy(
                R"([{"hostname":"198.51.100.128/25","ip":""}])", error));

            const QVariantMap routes = repository.vpnSites(RouteMode::VpnAllExceptSites);
            QCOMPARE(routes.size(), 3);
            QVERIFY(routes.contains("192.0.2.0/24"));
            QVERIFY(routes.contains("203.0.113.0/25"));
            QVERIFY(routes.contains("198.51.100.128/25"));
            QVERIFY(!routes.contains("198.51.100.0/24"));
            QVERIFY(!controller.removeSite("198.51.100.128/25"));

            QVERIFY(controller.importManagedBypassPolicy("[]", error));
            QCOMPARE(repository.vpnSites(RouteMode::VpnAllExceptSites).size(), 2);
            QVERIFY(!controller.hasManagedBypassSites());
            QVERIFY(controller.importManagedBypassPolicy(
                R"([{"hostname":"198.51.100.128/25","ip":""}])", error));

            QVERIFY(controller.importSitesFromJson(
                R"([{"hostname":"203.0.113.64/26","ip":""}])", true, error));
            QCOMPARE(repository.vpnSites(RouteMode::VpnAllExceptSites).size(), 2);
            QVERIFY(repository.vpnSites(RouteMode::VpnAllExceptSites).contains("203.0.113.64/26"));
            QVERIFY(repository.vpnSites(RouteMode::VpnAllExceptSites).contains("198.51.100.128/25"));

            controller.removeSites();
            QCOMPARE(repository.vpnSites(RouteMode::VpnAllExceptSites).size(), 1);
            QVERIFY(repository.vpnSites(RouteMode::VpnAllExceptSites).contains("198.51.100.128/25"));
        }

        IpSplitTunnelingController restored(&repository);
        QVERIFY(restored.hasManagedBypassSites());
        QVERIFY(restored.isManagedBypassSite("198.51.100.128/25"));
        QCOMPARE(repository.vpnSites(RouteMode::VpnAllExceptSites).size(), 1);
        settings.clearSettings();
    }
};

QTEST_MAIN(TestUiIpModelAndController)
#include "testUiIpModelAndController.moc"
