#include "chatclient.h"
#include <QDataStream>
#include <QJsonObject>
#include <QJsonDocument>


ChatClient::ChatClient(QObject *parent)
    : QObject{parent}
{
    m_clientSocket = new QTcpSocket(this);

    connect(m_clientSocket, &QTcpSocket::connected, this, &ChatClient::connected);
    connect(m_clientSocket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
}

void ChatClient::onReadyRead()
{
    QByteArray jsonData;
    QDataStream socketStream(m_clientSocket);
    socketStream.setVersion(QDataStream::Qt_5_12);
    // start an infinite loop
    for (;;) {
        socketStream.startTransaction();
        socketStream >> jsonData;
        if (socketStream.commitTransaction()) {
            // emit messageReceived(QString::fromUtf8(jsonData));

            QJsonParseError parseError;
            const QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
            if (parseError.error == QJsonParseError::NoError) {
                if (jsonDoc.isObject()) { // and is a JSON object
                    // emit logMessage(QJsonDocument(jsonDoc).toJson(QJsonDocument::Compact));
                    emit jsonReceived(jsonDoc.object()); // parse the JSON
                }
            }

        } else {
            break;
        }
    }
}

void ChatClient::sendMessage(const QString &text, const QString &type)
{
    if (m_clientSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "发送失败：客户端未连接";
        return;
    }

    if (!text.isEmpty()) {
        QDataStream serverStream(m_clientSocket);
        serverStream.setVersion(QDataStream::Qt_5_12);

        qDebug() << "发送消息，类型:" << type << "内容:" << text;

        if (type == "json") {
            // 直接发送JSON字符串
            QByteArray data = text.toUtf8();
            serverStream << data;
            qDebug() << "发送JSON数据长度:" << data.length();
        } else {
            // 否则创建JSON对象
            QJsonObject message;
            message["type"] = type;
            message["text"] = text;
            QByteArray data = QJsonDocument(message).toJson();
            serverStream << data;
            qDebug() << "发送普通消息数据长度:" << data.length();
        }
    }
}

void ChatClient::connectToServer(const QHostAddress &address, quint16 port)
{
    m_clientSocket->connectToHost(address, port);
}

void ChatClient::disconnectFromHost()
{
    m_clientSocket->disconnectFromHost();
}
