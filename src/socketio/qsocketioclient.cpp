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

QSocketIoClient::QSocketIoClient(QObject *parent) :
  QObject(parent),
  m_pWebSocket(new QWebSocket()),
  m_pNetworkAccessManager(new QNetworkAccessManager()),
  m_requestUrl(),
  m_connectionTimeout(30000),
  m_heartBeatTimeout(20000),
  m_pHeartBeatTimer(new QTimer()),
  m_sessionId()
{
  m_pHeartBeatTimer->setInterval(m_heartBeatTimeout);

  connect(m_pWebSocket, SIGNAL(error(QAbstractSocket::SocketError)),
	  this, SLOT(onError(QAbstractSocket::SocketError)));
  connect(m_pWebSocket, SIGNAL(textMessageReceived(QString)),
	  this, SLOT(onMessage(QString)));

  connect(m_pNetworkAccessManager, SIGNAL(finished(QNetworkReply*)),
	  this, SLOT(replyFinished(QNetworkReply*)));

  connect(m_pHeartBeatTimer, SIGNAL(timeout()), this, SLOT(sendHeartBeat()));
}

QSocketIoClient::~QSocketIoClient()
{
  m_pHeartBeatTimer->stop();
  delete m_pHeartBeatTimer;
  delete m_pWebSocket;
  delete m_pNetworkAccessManager;
}

bool QSocketIoClient::open(const QUrl &url)
{
  m_requestUrl = url;
  QUrl requestUrl(QStringLiteral("http://%1:%2/socket.io/1/?t=%3")
		  .arg(url.host())
		  .arg(QString::number(url.port(80)))
		  .arg(QString::number(QDateTime::currentMSecsSinceEpoch())));
  QNetworkRequest request(requestUrl);
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/html"));
  request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("*/*"));
  request.setRawHeader(QByteArrayLiteral("Connection"), QByteArrayLiteral("close"));
  m_pNetworkAccessManager->get(request);
  return true;
}

void QSocketIoClient::onError(QAbstractSocket::SocketError error)
{
  qDebug() << "Error occurred: " << error;
  // qDebug() << m_pWebSocket->errorString();
}

void QSocketIoClient::onMessage(QString textMessage)
{
  Q_UNUSED(textMessage);
  parseMessage(textMessage);
}

void QSocketIoClient::sendHeartBeat()
{
  (void)m_pWebSocket->sendTextMessage(QStringLiteral("2::"));
}

void QSocketIoClient::replyFinished(QNetworkReply *reply)
{
  int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  //QString statusReason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
  switch (status)
    {
    case 200:
      {
	//everything allright
	QString payload = QString::fromUtf8(reply->readAll());
	QStringList handshakeReturn = payload.split(QStringLiteral(":"));
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

	    QStringList protocols = handshakeReturn[3].split(QStringLiteral(","));
	    if (!protocols.contains(QStringLiteral("websocket")))
	      {
		qDebug() << "websockets not supported; so cannot continue";
		return;
	      }
	    m_sessionId = sessionId;
	    handshakeSucceeded();
	  }
	break;
      }

    case 401:	//unauthorized
      {
	//the server refuses to authorize the client to connect,
	//based on the supplied information (eg: Cookie header or custom query components).
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

void QSocketIoClient::handshakeSucceeded()
{
  m_requestUrl.setScheme(QStringLiteral("ws"));
  QUrl url(m_requestUrl.scheme() + "://" + m_requestUrl.host() + ":" +
	   QString::number(m_requestUrl.port()) +
	   QStringLiteral("/socket.io/1/websocket/") % m_sessionId);
  m_pWebSocket->open(url);
}

void QSocketIoClient::ackReceived(int messageId, QJsonArray arguments)
{
  QAbstractCallback *callback = m_callbacks.value(messageId, Q_NULLPTR);

  if (callback)
    {
      (*callback)(arguments);
      delete callback;
    }
}

#include <functional>

void QSocketIoClient::eventReceived(QString message, QJsonArray arguments,
                                    bool mustAck, int messageId)
{
  if (m_subscriptions.contains(message))
    {
      QJsonValue retVal;
      QAbstractCallback *callback = m_subscriptions[message];
      // if (callback->hasReturnValue())
      // 	retVal = (*callback)(arguments);
      // else
	(*callback)(arguments);
      if (mustAck)
	acknowledge(messageId, retVal);
    }
}

void QSocketIoClient::parseMessage(const QString &message)
{
  QRegExp regExp(QStringLiteral("^([^:]+):([0-9]+)?(\\+)?:([^:]+)?:?([\\s\\S]*)?$"),
		 Qt::CaseInsensitive, QRegExp::RegExp2);
  if (regExp.indexIn(message) != -1)
    {
      QStringList captured = regExp.capturedTexts();
      int messageType = captured.at(1).toInt();
      int messageId = captured.at(2).toInt();
      bool mustAck = (messageId != 0);
      bool autoAck = mustAck && captured.at(3).isEmpty();
      QString endpoint = captured.at(4);
      QString data = captured.at(5);

      if (autoAck)
	acknowledge(messageId);

      switch(messageType)
        {
	case 0:	//disconnect
	  {
	    Q_EMIT(disconnected(endpoint));
	    break;
	  }
	case 1: //connect
	  {
	    if (endpoint != m_requestUrl.path())
	      m_pWebSocket->sendTextMessage("1::" + m_requestUrl.path());
	    else
	      {
		m_pHeartBeatTimer->start();
		Q_EMIT(connected(endpoint));
	      }
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
	    QJsonDocument document = QJsonDocument::fromJson(QByteArray(data.toLatin1()),
							     &parseError);
	    if (parseError.error != QJsonParseError::NoError)
	      qDebug() << parseError.errorString();
	    else
	      {
		if (document.isObject())
		  {
		    QJsonObject object = document.object();
		    QJsonValue value = object[QStringLiteral("name")];
		    if (!value.isUndefined())
		      {
			QString message = value.toString();
			QJsonArray arguments;
			QJsonValue argsValue = object[QStringLiteral("args")];
			if (!argsValue.isUndefined() && !argsValue.isNull())
			  {
			    if (argsValue.isArray())
			      arguments = argsValue.toArray();
			    else
			      {
				qWarning() << "Args argument is not an array";
				return;
			      }
			  }
			eventReceived(message, arguments, mustAck && !autoAck, messageId);
		      }
		    else
		      qWarning() << "Invalid event received: no name";
		  }
	      }
	    break;
	  }
	case 6:	//ack
	  {
	    QRegExp regExp(QStringLiteral("^([0-9]+)(\\+)?(.*)$"),
			   Qt::CaseInsensitive, QRegExp::RegExp2);
	    if (regExp.indexIn(data) != -1)
	      {
		QJsonParseError parseError;
		QJsonArray arguments;
		int messageId = regExp.cap(1).toInt();
		QString argumentsValue = regExp.cap(3);
		if (!argumentsValue.isEmpty())
		  {
		    QJsonDocument doc = QJsonDocument::fromJson(argumentsValue.toLatin1(),
								&parseError);
		    if (parseError.error != QJsonParseError::NoError)
		      {
			qWarning() << "JSONParseError:" << parseError.errorString();
			return;
		      }
		    else
		      {
			if (doc.isArray())
			  arguments = doc.array();
			else
			  {
			    qWarning() << "Error: data of event is not an array";
			    return;
			  }
		      }
		  }
		ackReceived(messageId, arguments);
	      }
	    break;
	  }
	case 7:	//error
	  {
	    QStringList pieces = data.split(QStringLiteral("+"));
	    QString reason = pieces[0];
	    QString advice;
	    if (pieces.length() == 2)
	      advice = pieces[1];
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

QJsonDocument QSocketIoClient::package(const QVariant &value)
{
  if (value.canConvert<QVariantMap>())
    return QJsonDocument(QJsonObject::fromVariantMap(value.toMap()));
  else if (value.canConvert<QVariantList>())
    return QJsonDocument(QJsonArray::fromVariantList(value.toList()));
  else
    {
      QJsonArray ar;
      ar.append(QJsonValue::fromVariant(value));
      return QJsonDocument(ar);
    }
}

void QSocketIoClient::emitMessage(const QString &message, bool value)
{
  const QString m_endPoint(m_requestUrl.path());
  doEmitMessage(message, package(value), m_endPoint, true);
}

void QSocketIoClient::emitMessage(const QString &message, int value)
{
  const QString m_endPoint(m_requestUrl.path());
  doEmitMessage(message, package(value), m_endPoint, true);
}

void QSocketIoClient::emitMessage(const QString &message, double value)
{
  const QString m_endPoint(m_requestUrl.path());
  doEmitMessage(message, package(value), m_endPoint, true);
}

void QSocketIoClient::emitMessage(const QString &message, const QString &value)
{
  const QString m_endPoint(m_requestUrl.path());
  doEmitMessage(message, package(value), m_endPoint, true);
}

void QSocketIoClient::emitMessage(const QString &message, const QVariantList &arguments)
{
  const QString m_endPoint(m_requestUrl.path());
  doEmitMessage(message, package(arguments), m_endPoint, true);
}

void QSocketIoClient::emitMessage(const QString &message, const QVariantMap &arguments)
{
  const QString m_endPoint(m_requestUrl.path());
  doEmitMessage(message, package(arguments), m_endPoint, true);
}

QString QSocketIoClient::sessionId() const
{
  return m_sessionId;
}

void QSocketIoClient::acknowledge(int messageId, const QJsonValue &retVal)
{
  QString msg = QStringLiteral("6:::") + QString::number(messageId);
  if (!retVal.isUndefined() && !retVal.isNull())
    {
      QJsonDocument doc;
      if (retVal.isArray())
	doc.setArray(retVal.toArray());
      else if (retVal.isObject())
	doc.setObject(retVal.toObject());
      else
	{
	  QJsonArray ar;
	  ar.append(retVal);
	  doc.setArray(ar);
	}
      msg.append(QStringLiteral("+") + QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }
  (void)m_pWebSocket->sendTextMessage(msg);
}

int QSocketIoClient::doEmitMessage(const QString &message, const QJsonDocument &document,
				   const QString &endpoint, bool callbackExpected)
{
  static int id = 0;
  const QString msg = QStringLiteral("5:%1%2:%3:{\"name\":\"%4\",\"args\":%5}")
    .arg(++id)
    .arg(callbackExpected ? QStringLiteral("+") : QStringLiteral(""))
    .arg(endpoint)
    .arg(message)
    .arg(QString::fromUtf8(document.toJson(QJsonDocument::Compact)));
  (void)m_pWebSocket->sendTextMessage(msg);
  return id;
}
