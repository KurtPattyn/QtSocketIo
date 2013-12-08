#ifndef QSOCKETIOCLIENT_H
#define QSOCKETIOCLIENT_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QStringList>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include "QtWebSockets/QWebSocket"
#include "qsocketio_global.h"
#include "qcallback.h"

QT_BEGIN_NAMESPACE

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class Q_SOCKETIO_EXPORT QSocketIoClient : public QObject
{
    Q_OBJECT
public:
    explicit QSocketIoClient(QObject *parent = Q_NULLPTR);
    virtual ~QSocketIoClient();

    bool open(const QUrl &url);
    //TODO: close() function

    void emitMessage(const QString &message, bool value);
    void emitMessage(const QString &message, int value);
    void emitMessage(const QString &message, double value);
    void emitMessage(const QString &message, const QString &value);
    void emitMessage(const QString &message, const QVariantList &arguments);
    void emitMessage(const QString &message, const QVariantMap &arguments);

    template <typename Callback>
    void emitMessage(const QString &message, bool value, Callback callback);
    template <typename Callback>
    void emitMessage(const QString &message, int value, Callback callback);
    template <typename Callback>
    void emitMessage(const QString &message, double value, Callback callback);
    template <typename Callback>
    void emitMessage(const QString &message, const QString &value, Callback callback);
    template <typename Callback>
    void emitMessage(const QString &message, const QVariantList &value, Callback callback);
    template <typename Callback>
    void emitMessage(const QString &message, const QVariantMap &value, Callback callback);

    void on(const QString &event, const QObject *receiver, const char *member, Qt::ConnectionType);
    template <typename Callback>
    typename std::enable_if<function_traits<Callback>::is_function, void>::type
    on(const QString &event, Callback callback);

    QString sessionId() const;

Q_SIGNALS:
    void messageReceived(QString message);
    void errorReceived(QString reason, QString advice);
    void connected(QString endpoint);
    void disconnected(QString endpoint);
    void heartbeatReceived();

private Q_SLOTS:
    void onError(QAbstractSocket::SocketError error);
    void onMessage(QString textMessage);

    void sendHeartBeat();

    void replyFinished(QNetworkReply *reply);

private:
    QWebSocket *m_pWebSocket;
    QNetworkAccessManager *m_pNetworkAccessManager;
    QUrl m_requestUrl;
    qint32 m_connectionTimeout;
    qint32 m_heartBeatTimeout;
    QTimer *m_pHeartBeatTimer;
    QString m_sessionId;
    QMap<int, QAbstractCallback *> m_callbacks;
    QMap<QString, QAbstractCallback *> m_subscriptions;

    void parseMessage(const QString &message);
    QJsonDocument package(const QVariant &value);
    int doEmitMessage(const QString &message, const QJsonDocument &document,
                      const QString &endpoint, bool callbackExpected);

    void acknowledge(int messageId, const QJsonValue &retVal = QJsonValue());

    void handshakeSucceeded();
    void ackReceived(int messageId, QJsonArray arguments);
    void eventReceived(QString message, QJsonArray arguments, bool mustAck, int messageId);
};

template <typename Callback>
typename std::enable_if<function_traits<Callback>::is_function, void>::type
QSocketIoClient::on(const QString &event, Callback callback)
{
    m_subscriptions.insert(event, new FunctionCallback<Callback>(callback));
}

template <typename Callback>
void QSocketIoClient::emitMessage(const QString &message,
                                  bool value, Callback callback)
{
    int id = doEmitMessage(message, package(value), QString(), true);
    m_callbacks.insert(id, new FunctionCallback<Callback>(callback));
}

template <typename Callback>
void QSocketIoClient::emitMessage(const QString &message,
                                  int value, Callback callback)
{
    int id = doEmitMessage(message, package(value), QString(), true);
    m_callbacks.insert(id, new FunctionCallback<Callback>(callback));
}

template <typename Callback>
void QSocketIoClient::emitMessage(const QString &message,
                                  double value, Callback callback)
{
    int id = doEmitMessage(message, package(value), QString(), true);
    m_callbacks.insert(id, new FunctionCallback<Callback>(callback));
}

template <typename Callback>
void QSocketIoClient::emitMessage(const QString &message,
                                  const QString &value, Callback callback)
{
    int id = doEmitMessage(message, package(value), QString(), true);
    m_callbacks.insert(id, new FunctionCallback<Callback>(callback));
}

template <typename Callback>
void QSocketIoClient::emitMessage(const QString &message,
                                  const QVariantList &value, Callback callback)
{
    int id = doEmitMessage(message, package(value), QString(), true);
    m_callbacks.insert(id, new FunctionCallback<Callback>(callback));
}

template <typename Callback>
void QSocketIoClient::emitMessage(const QString &message,
                                  const QVariantMap &value, Callback callback)
{
    int id = doEmitMessage(message, package(value), QString(), true);
    m_callbacks.insert(id, new FunctionCallback<Callback>(callback));
}

QT_END_NAMESPACE

#endif // QSOCKETIOCLIENT_H
