#ifndef VDSPANELCLIENT_H
#define VDSPANELCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>

class VdsPanelClient : public QObject
{
    Q_OBJECT

public:
    explicit VdsPanelClient(QObject *parent = nullptr);

public slots:
    void fetchOwnProfile(const QString &panelUrl, const QString &username,
                         const QString &password);

signals:
    void roleReady(const QString &role);
    void profileReady(const QString &profile);
    void errorOccurred(const QString &message);

private:
    QNetworkAccessManager m_network;
    QUrl m_baseUrl;
    bool m_busy = false;

    QUrl endpoint(const QString &path) const;
    void fetchIdentity();
    void fetchProfile();
    void fail(const QString &message);
};

#endif
