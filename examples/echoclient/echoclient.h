#ifndef ECHOCLIENT_H
#define ECHOCLIENT_H

#include <QtCore/QObject>
#include <QtSocketIo/QSocketIoClient>

class EchoClient : public QObject
{
    Q_OBJECT
public:
    explicit EchoClient(QObject *parent = Q_NULLPTR);
    virtual ~EchoClient();

    void open(QUrl url);

public Q_SLOTS:
    void messageReceived(QString message);
    void errorReceived(QString reason, QString advice);
    void ackReceived(int messageId, QJsonArray arguments);
    void connected(QString endpoint);
    void disconnected(QString endpoint);
    void heartbeatReceived();

private:
    QSocketIoClient m_client;
};

#endif // ECHOCLIENT_H
