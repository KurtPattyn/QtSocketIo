#ifndef QSOCKETIOCLIENT_H
#define QSOCKETIOCLIENT_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QStringList>
#include <QtCore/QJsonArray>
#include "QtWebSockets/QWebSocket"

QT_BEGIN_NAMESPACE

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class QSocketIOClient : public QObject
{
    Q_OBJECT
public:
    explicit QSocketIOClient(QObject *parent = Q_NULLPTR);

    bool open(const QUrl &url, bool masking = true);
    void emitMessage(const QString &message, const QVariantList &arguments);
    void emitMessage(const char *message, const QVariantList &arguments);

    QString sessionId() const;

Q_SIGNALS:
    void eventReceived(QString message, QJsonArray arguments);
    void messageReceived(QString message);
    void errorReceived(QString reason, QString advice);
    void ackReceived(int messageId, QJsonArray arguments);
    void connected(QString endpoint);
    void disconnected(QString endpoint);
    void heartbeatReceived();

    //TODO: remove? otherwise it is available to outside clients; do we want this?
    void handshakeSucceeded();

public Q_SLOTS:

private Q_SLOTS:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onTextMessage(QString textMessage);
    void onBinaryMessage(QByteArray binaryMessage);

    void sendHeartBeat();

    void replyFinished(QNetworkReply *reply);

    void onHandshakeSucceeded();

    void onHeartbeatReceived();

private:
    QWebSocket *m_pWebSocket;
    QNetworkAccessManager *m_pNetworkAccessManager;
    QUrl m_requestUrl;
    bool m_mustMask;
    qint32 m_connectionTimeout;
    qint32 m_heartBeatTimeout;
    QTimer *m_pHeartBeatTimer;
    QString m_sessionId;

    void parseMessage(const QString &message);

    void acknowledge(int messageId, const QVariantList &arguments = QVariantList());
};

QT_END_NAMESPACE

#endif // QSOCKETIOCLIENT_H
