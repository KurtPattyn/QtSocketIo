#include "qsocketioclient.h"
#include <QtWebSockets/QWebSocket>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QRegExp>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QDebug>
#include <functional>

QSocketIOClient::QSocketIOClient(QObject *parent) :
    QObject(parent),
    m_pWebSocket(Q_NULLPTR),
    m_pNetworkAccessManager(Q_NILLPTR),
    m_requestUrl(),
    m_mustMask(true),
    m_connectionTimeout(30000),
    m_heartBeatTimeout(20000),
    m_pHeartBeatTimer(Q_NULLPTR),
    m_sessionId()
{
    m_pWebSocket = new QWebSocket(QString(), QWebSocketProtocol::V_LATEST, this);
    m_pNetworkAccessManager = new QNetworkAccessManager(this);
    m_pHeartBeatTimer = new QTimer(this);
    m_pHeartBeatTimer->setInterval(m_heartBeatTimeout);

    connect(m_pWebSocket, SIGNAL(connected()), this,SLOT(onConnected()));
    connect(m_pWebSocket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    connect(m_pWebSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onError(QAbstractSocket::SocketError)));
    connect(m_pWebSocket, SIGNAL(textMessageReceived(QString)), this, SLOT(onTextMessage(QString)));
    connect(m_pWebSocket, SIGNAL(binaryMessageReceived(QByteArray)), this, SLOT(onBinaryMessage(QByteArray)));

    connect(m_pNetworkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));

    connect(m_pHeartBeatTimer, SIGNAL(timeout()), this, SLOT(sendHeartBeat()));

    //socket.io connections
    connect(this, SIGNAL(handshakeSucceeded()), this, SLOT(onHandshakeSucceeded()));

    //socket.io test connections
    connect(this, SIGNAL(heartbeatReceived()), this, SLOT(onHeartbeatReceived()));
}

bool QSocketIOClient::open(const QUrl &url, bool masking)
{
    m_requestUrl = url;
    m_mustMask = masking;
    QUrl requestUrl("http://" + url.host() + ":" + QString::number(url.port(80)) + "/socket.io/1/?t=" + QString::number(QDateTime::currentMSecsSinceEpoch()));
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/html");
    request.setRawHeader("Accept", "*/*");
    request.setRawHeader("Connection", "close");
    m_pNetworkAccessManager->post(request, QByteArray());
    return true;
}

void QSocketIOClient::onConnected()
{
    qDebug() << "WebSocket connected";
}

void QSocketIOClient::onDisconnected()
{
    qDebug() << "Socket disconnected";
}

void QSocketIOClient::onError(QAbstractSocket::SocketError error)
{
    qDebug() << "Error occurred: " << error;
}

void QSocketIOClient::onTextMessage(QString textMessage)
{
    Q_UNUSED(textMessage);
    parseMessage(textMessage);
}

void QSocketIOClient::onBinaryMessage(QByteArray binaryMessage)
{
    Q_UNUSED(binaryMessage);
    qDebug() << "Binary message received";
}

void QSocketIOClient::sendHeartBeat()
{
    qDebug() << "Sending heartbeat";
    m_pWebSocket->write("2::");
}

void QSocketIOClient::replyFinished(QNetworkReply *reply)
{
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    //QString statusReason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    //qDebug() << "Reply finished: " << status << statusReason;
    switch (status)
    {
        case 200:
        {
            //everything allright
            QString payload = QString::fromUtf8(reply->readAll());
            QStringList handshakeReturn = payload.split(":");
            if (handshakeReturn.length() != 4)
            {
                qDebug() << "Not a valid handshake return";
            }
            else
            {
                QString sessionId = handshakeReturn[0];
                m_heartBeatTimeout = handshakeReturn[1].toInt() * 1000 - 500;
                m_connectionTimeout = handshakeReturn[2].toInt() * 1000;

                m_pHeartBeatTimer->setInterval(m_heartBeatTimeout);

                QStringList protocols = handshakeReturn[3].split(",");
                if (!protocols.contains("websocket"))
                {
                    qDebug() << "websockets not supported; so cannot continue";
                    return;
                }
                //qDebug() << "sessionID:" << sessionId << "heartBeatTimeout:" << heartBeatTimeout << "s connectionTimeout:" << connectionTimeout << "s";
                m_sessionId = sessionId;
                Q_EMIT(handshakeSucceeded());
            }
            break;
        }

        case 401:	//unauthorized
        {
            //the server refuses to authorize the client to connect, based on the supplied information (eg: Cookie header or custom query components).
            qDebug() << "Error:" << reply->readAll();
            break;
        }

        case 500:	//internal server error
        {
            qDebug() << "Error:" << reply->readAll();
            break;
        }

        case 404:	//Not Found
        {
            qDebug() << "Error: " << reply->readAll();
        }

        case 503:	//service unavailable
        {
            //the server refuses the connection for any reason (e.g. overload)
            qDebug() << "Error:" << reply->readAll();
            break;
        }

        default:
        {
        }
    }
}

void QSocketIOClient::onHandshakeSucceeded()
{
    QUrl url(m_requestUrl.toString()+ QStringLiteral("/socket.io/1/websocket/") + m_sessionId);
    m_pWebSocket->open(url, m_mustMask);
}

void QSocketIOClient::onHeartbeatReceived()
{
    qDebug() << "heartbeat received";
}

void QSocketIOClient::parseMessage(const QString &message)
{
    //qDebug() << "SocketIOClient::parseMessage" << message;
    QRegExp regExp("^([^:]+):([0-9]+)?(\\+)?:([^:]+)?:?([\\s\\S]*)?$", Qt::CaseInsensitive, QRegExp::RegExp2);
    if (regExp.indexIn(message) != -1)
    {
        QStringList captured = regExp.capturedTexts();
        //qDebug() << "Found:" << regExp.capturedTexts();
        int messageType = captured.at(1).toInt();
        int messageId = captured.at(2).toInt();
        bool mustAck = (messageId != 0);
        bool autoAck = mustAck && captured.at(3).isEmpty();
        QString endpoint = captured.at(4);
        QString data = captured.at(5);
        /*qDebug() << "MessageType:" << messageType << "MessageId:" << messageId <<
                    "autoAck:" << autoAck << "mustAck:" << mustAck << "endpoint:" << endpoint <<
                    "data:" << data;*/

        if (autoAck)
        {
            acknowledge(messageId);
        }

        switch(messageType)
        {
            case 0:	//disconnect
            {
                Q_EMIT(disconnected(endpoint));
                break;
            }
            case 1: //connect
            {
                m_pHeartBeatTimer->start();
                Q_EMIT(connected(endpoint));
                break;
            }
            case 2:	//heartbeat
            {
                Q_EMIT(heartbeatReceived());
                break;
            }
            case 3:	//message
            {
                Q_EMIT(messageReceived(data));
                break;
            }
            case 4:	//json message
            {
                qDebug() << "JSON message received:" << data;
                break;
            }
            case 5: //event
            {
                QJsonParseError parseError;
                //qDebug() << "Event received:" << data;
                QJsonDocument document = QJsonDocument::fromJson(QByteArray(data.toLatin1()), &parseError);
                if (parseError.error != QJsonParseError::NoError)
                {
                    qDebug() << parseError.errorString();
                }
                else
                {
                    if (document.isObject())
                    {
                        QJsonObject object = document.object();
                        QJsonValue value = object["name"];
                        if (!value.isUndefined())
                        {
                            QString message = value.toString();
                            //QVariantList arguments;
                            QJsonArray arguments;
                            QJsonValue argsValue = object["args"];
                            if (!argsValue.isUndefined() && !argsValue.isNull())
                            {
                                if (argsValue.isArray())
                                {
                                    //QJsonArray argsArray = argsValue.toArray();
                                    //arguments = argsArray.toVariantList();
                                    arguments = argsValue.toArray();
                                    //qDebug() << "Received arguments" << argsValue;
                                    /*Q_FOREACH(QJsonValue val, argsArray)
                                    {
                                        arguments << val.toVariant();
                                    }*/
                                }
                                else
                                {
                                    qDebug() << "Args argument is not an array";
                                    return;
                                }
                            }
                            Q_EMIT(eventReceived(message, arguments));
                        }
                        else
                        {
                            qDebug() << "Invalid event received: no name";
                        }
                    }
                }
                break;
            }
            case 6:	//ack
            {
                QRegExp regExp("^([0-9]+)(\\+)?(.*)$", Qt::CaseInsensitive, QRegExp::RegExp2);
                if (regExp.indexIn(data) != -1)
                {
                    QJsonParseError parseError;
                    //QVariantList arguments;
                    QJsonArray arguments;
                    int messageId = regExp.cap(1).toInt();
                    QString argumentsValue = regExp.cap(3);
                    if (!argumentsValue.isEmpty())
                    {
                        QJsonDocument doc = QJsonDocument::fromJson(argumentsValue.toLatin1(), &parseError);
                        if (parseError.error != QJsonParseError::NoError)
                        {
                            qDebug() << "JSONParseError:" << parseError.errorString();
                            return;
                        }
                        else
                        {
                            if (doc.isArray())
                            {
                                //arguments = doc.array().toVariantList();
                                arguments = doc.array();
                            }
                            else
                            {
                                qDebug() << "Error: data of event is not an array";
                                return;
                            }
                        }
                    }
                    Q_EMIT(ackReceived(messageId, arguments));
                }
                break;
            }
            case 7:	//error
            {
                QStringList pieces = data.split("+");
                QString reason = pieces[0];
                QString advice;
                if (pieces.length() == 2)
                {
                    advice = pieces[1];
                }
                Q_EMIT(errorReceived(reason, advice));
                break;
            }
            case 8:	//noop
            {
                qDebug() << "Noop received" << data;
                break;
            }
        }
    }
}

void QSocketIOClient::emitMessage(const QString &message, const QVariantList &arguments)
{
    const QString m_endPoint;
    const QJsonArray ja = QJsonArray::fromVariantList(arguments);
    const QJsonDocument doc(ja);
    const QString msg = QStringLiteral("5::") + m_endPoint + QStringLiteral(":{\"name\":\"") + message + QStringLiteral("\",\"args\":") + doc.toJson() + QStringLiteral("}");
    m_pWebSocket->write(msg);
}

void QSocketIOClient::emitMessage(const char *message, const QVariantList &arguments)
{
    emitMessage(QString::fromLatin1(message), arguments);
}

QString QSocketIOClient::sessionId() const
{
    return m_sessionId;
}

void QSocketIOClient::acknowledge(int messageId, const QVariantList &arguments)
{
    QString msg = QStringLiteral("6:::") + QString::number(messageId);
    if (!arguments.isEmpty())
    {
        const QJsonArray ja = QJsonArray::fromVariantList(arguments);
        const QJsonDocument doc(ja);
        msg.append(QStringLiteral("+") + doc.toJson());
    }
    m_pWebSocket->write(msg);
}
