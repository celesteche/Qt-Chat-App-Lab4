#include "serverworker.h"
#include <QDataStream>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHostAddress>
#include <QDebug>

ServerWorker::ServerWorker(QObject *parent)
    : QObject{parent}
{
    m_serverSocket = new QTcpSocket(this);
    connect(m_serverSocket, &QTcpSocket::readyRead, this, &ServerWorker::onReadyRead);
    connect(m_serverSocket, &QTcpSocket::disconnected, this, &ServerWorker::disconnectedFromClient);
}

bool ServerWorker::setSocketDescriptor(qintptr socketDescriptor)
{
    return m_serverSocket->setSocketDescriptor(socketDescriptor);
}

QString ServerWorker::userName()
{
    return m_userName;
}

void ServerWorker::setUserName(QString user)
{
    m_userName = user;
}

void ServerWorker::disconnectFromClient()
{
    if (m_serverSocket->state() == QAbstractSocket::ConnectedState) {
        m_serverSocket->disconnectFromHost();
    }
}

QString ServerWorker::peerAddress() const
{
    if (m_serverSocket) {
        QString ip = m_serverSocket->peerAddress().toString();
        // 去掉IPv6的前缀（如果有）
        if (ip.startsWith("::ffff:")) {
            ip = ip.mid(7); // 去掉 "::ffff:"
        }
        return ip + ":" + QString::number(m_serverSocket->peerPort());
    }
    return "Unknown";
}

void ServerWorker::onReadyRead()
{
    QByteArray jsonData;
    QDataStream socketStream(m_serverSocket);
    socketStream.setVersion(QDataStream::Qt_5_12);
    // start an infinite loop
    for (;;) {
        socketStream.startTransaction();
        socketStream >> jsonData;
        if (socketStream.commitTransaction()) {
            // emit logMessage(QString::fromUtf8(jsonData));
            // sendMessage("I recieved message");

            QJsonParseError parseError;
            const QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
            if (parseError.error == QJsonParseError::NoError) {
                if (jsonDoc.isObject()) { // and is a JSON object
                    emit logMessage(QJsonDocument(jsonDoc).toJson(QJsonDocument::Compact));
                    emit jsonReceived(this, jsonDoc.object()); // parse the JSON
                }
            }

        } else {
            break;
        }
    }
}

void ServerWorker::sendMessage(const QString &text, const QString &type)
{
    if (m_serverSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "发送失败：套接字未连接";
        return;
    }

    if (!text.isEmpty()) {
        // create a QDataStream operating on the socket
        QDataStream serverStream(m_serverSocket);
        serverStream.setVersion(QDataStream::Qt_5_12);

        // Create the JSON we want to send
        QJsonObject message;
        message["type"] = type;
        message["text"] = text;

        qDebug() << "发送消息:" << type << text;

        // send the JSON using QDataStream
        serverStream << QJsonDocument(message).toJson();
    }
}

bool ServerWorker::sendJson(const QJsonObject &json)
{
    const QByteArray jsonData = QJsonDocument(json).toJson(QJsonDocument::Compact);
    QString jsonStr = QString::fromUtf8(jsonData);

    qDebug() << "准备发送JSON给" << userName() << ":" << jsonStr;

    if (m_serverSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "发送失败：套接字未连接";
        emit logMessage(QLatin1String("发送失败给 ") + userName() + QLatin1String(" - 套接字未连接"));
        return false;
    }

    QDataStream socketStream(m_serverSocket);
    socketStream.setVersion(QDataStream::Qt_5_7);

    bool result = false;
    try {
        socketStream << jsonData;
        result = true;
        qDebug() << "发送成功，数据长度:" << jsonData.length();
        emit logMessage(QLatin1String("成功发送给 ") + userName() + QLatin1String(" - ") + jsonStr);
    } catch (...) {
        qDebug() << "发送失败：写入套接字时发生异常";
        emit logMessage(QLatin1String("发送失败给 ") + userName() + QLatin1String(" - 写入异常"));
        result = false;
    }

    return result;
}
