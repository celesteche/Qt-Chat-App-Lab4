#include "chatserver.h"
#include "serverworker.h"
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

ChatServer::ChatServer(QObject *parent)
    : QTcpServer{parent}
{
    m_threadPool = new ThreadPoolManager(this);

    // 连接线程池相关的信号槽
    connect(this, &ChatServer::broadcastMessage, this, &ChatServer::onBroadcastMessage);
    connect(this, &ChatServer::handleNewConnection, this, &ChatServer::onHandleNewConnection);
}

ChatServer::~ChatServer()
{
    // 清理所有客户端连接
    for (ServerWorker *worker : m_clients) {
        worker->deleteLater();
    }
    m_clients.clear();

    if (m_threadPool) {
        delete m_threadPool;
    }
}

void ChatServer::incomingConnection(qintptr socketDescriptor)
{
    // 使用线程池处理新连接
    m_threadPool->startConnectionTask(this, socketDescriptor);
    emit logMessage("新的连接请求已放入线程池处理");
}

void ChatServer::onHandleNewConnection(qintptr socketDescriptor)
{
    ServerWorker *worker = new ServerWorker(this);
    if (!worker->setSocketDescriptor(socketDescriptor)) {
        worker->deleteLater();
        emit logMessage("设置套接字描述符失败");
        return;
    }

    connect(worker, &ServerWorker::logMessage, this, &ChatServer::logMessage);
    connect(worker, &ServerWorker::jsonReceived, this, &ChatServer::jsonReceived);
    connect(worker, &ServerWorker::disconnectedFromClient, this, std::bind(&ChatServer::userDisconnected, this, worker));
    m_clients.append(worker);

    QString logMsg = QString("新的用户连接上了 (线程池活动线程: %1)").arg(m_threadPool->activeThreadCount());
    emit logMessage(logMsg);
}

void ChatServer::broadcast(const QJsonObject &message, ServerWorker *exclude)
{
    // 使用线程池处理广播
    m_threadPool->startMessageTask(this, message, exclude);
}

void ChatServer::onBroadcastMessage(const QJsonObject &message, ServerWorker *exclude)
{
    // 在线程中执行实际的广播操作
    for (ServerWorker *worker : m_clients) {
        if (worker != exclude) {
            worker->sendJson(message);
        }
    }
}

void ChatServer::stopServer()
{
    emit logMessage("正在停止服务器...");

    // 通知所有客户端服务器即将关闭
    QJsonObject shutdownMessage;
    shutdownMessage["type"] = "shutdown";
    shutdownMessage["text"] = "服务器正在关闭";
    broadcast(shutdownMessage, nullptr);

    // 关闭所有客户端连接
    for (ServerWorker *worker : m_clients) {
        worker->disconnectFromClient();
    }

    close();
    emit logMessage("服务器已停止");
}

void ChatServer::jsonReceived(ServerWorker *sender, const QJsonObject &docObj)
{
    const QJsonValue typeVal = docObj.value("type");
    if (typeVal.isNull() || !typeVal.isString())
        return;

    if (typeVal.toString().compare("message", Qt::CaseInsensitive) == 0) {
        const QJsonValue textVal = docObj.value("text");
        if (textVal.isNull() || !textVal.isString())
            return;
        const QString text = textVal.toString().trimmed();
        if (text.isEmpty())
            return;

        QJsonObject message;
        message["type"] = "message";
        message["text"] = text;
        message["sender"] = sender->userName();
        message["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss");

        broadcast(message, nullptr);

        // 记录消息到日志
        emit logMessage(QString("消息广播: %1 -> %2").arg(sender->userName()).arg(text));

    } else if (typeVal.toString().compare("login", Qt::CaseInsensitive) == 0) {
        const QJsonValue usernameVal = docObj.value("text");
        if (usernameVal.isNull() || !usernameVal.isString())
            return;

        sender->setUserName(usernameVal.toString());
        QJsonObject connectedMessage;
        connectedMessage["type"] = "newuser";
        connectedMessage["username"] = usernameVal.toString();

        broadcast(connectedMessage, sender);

        //send user list to new logined user
        QJsonObject userListMessage;
        userListMessage["type"] = "userlist";
        QJsonArray userlist;
        for (ServerWorker *worker : m_clients) {
            if (worker == sender)
                userlist.append(worker->userName() + "**");
            else
                userlist.append(worker->userName());
        }
        userListMessage["userlist"] = userlist;
        sender->sendJson(userListMessage);

        emit logMessage(QString("用户登录: %1 (在线用户: %2)").arg(usernameVal.toString()).arg(m_clients.size()));
    }
}

void ChatServer::userDisconnected(ServerWorker *sender)
{
    m_clients.removeAll(sender);
    const QString userName = sender->userName();
    if (!userName.isEmpty()) {
        QJsonObject disconnectedMessage;
        disconnectedMessage["type"] = "userdisconnected";
        disconnectedMessage["username"] = userName;
        broadcast(disconnectedMessage, nullptr);
    }
    emit logMessage(QString("%1 断开连接 (剩余用户: %2)").arg(userName).arg(m_clients.size()));
    sender->deleteLater();
}
