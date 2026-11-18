#ifndef VDSPANELCLIENT_H
#define VDSPANELCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QUrl>

class VdsPanelClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(QString role READ role NOTIFY roleChanged)
    Q_PROPERTY(QUrl panelUrl READ panelUrl NOTIFY roleChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    explicit VdsPanelClient(QObject *parent = nullptr);
    bool signedIn() const { return m_signedIn; }
    QString role() const { return m_role; }
    QUrl panelUrl() const { return m_baseUrl; }
    QString statusText() const { return m_statusText; }

public slots:
    void fetchOwnProfile(const QString &panelUrl, const QString &username,
                         const QString &password);
    void fetchStatus();

signals:
    void roleReady(const QString &role);
    void profileReady(const QString &profile);
    void errorOccurred(const QString &message);
    void signedInChanged();
    void roleChanged();
    void statusChanged();

private:
    QNetworkAccessManager m_network;
    QUrl m_baseUrl;
    bool m_busy = false;
    bool m_signedIn = false;
    bool m_statusActive = false;
    QString m_role;
    QString m_statusText;
    QTimer m_statusTimer;

    QUrl endpoint(const QString &path) const;
    void fetchIdentity();
    void fetchAdminIdentity();
    void fetchProfile();
    void fail(const QString &message);
};

#endif
