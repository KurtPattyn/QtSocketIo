#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <QObject>
#include <QUrl>
#include <QStringList>
#include <QJsonArray>
#include "websocket.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class SocketIOClient : public QObject
{
    Q_OBJECT
public:
    explicit SocketIOClient(QObject *parent = 0);

    bool open(const QUrl &url, bool masking = true);
    void emitMessage(QString message, const QVariantList &arguments);

    QString getSessionId() const;

Q_SIGNALS:
    //socket.io signals
    void eventReceived(QString message, QJsonArray arguments);
    void messageReceived(QString message);
    void errorReceived(QString reason, QString advice);
    void ackReceived(int messageId, QJsonArray arguments);
    void connected(QString endpoint);
    void disconnected(QString endpoint);
    void heartbeatReceived();

    //socket.io handshake
    //TODO: remove? otherwise it is available to outside clients; do we want this?
    void handshakeSucceeded();

public Q_SLOTS:

private Q_SLOTS:
    //WebSocket events
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onTextMessage(QString textMessage);
    void onBinaryMessage(QByteArray binaryMessage);

    void sendHeartBeat();

    void replyFinished(QNetworkReply *reply);

    //socket.io callbacks
    void onHandshakeSucceeded();

    //socket.io test callbacks
    void onHeartbeatReceived();

private:
    WebSocket *m_pWebSocket;
    QNetworkAccessManager *m_pNetworkAccessManager;
    QUrl m_requestUrl;
    bool m_mustMask;
    qint32 m_connectionTimeout;
    qint32 m_heartBeatTimeout;
    QTimer *m_pHeartBeatTimer;
    QString m_sessionId;

    void parseMessage(QString message);

    void acknowledge(int messageId, const QVariantList &arguments = QVariantList());
};

#endif // WEBSOCKETCLIENT_H
