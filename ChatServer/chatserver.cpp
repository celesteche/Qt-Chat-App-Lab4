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

// 在 jsonReceived 函数中添加私聊消息处理
void ChatServer::jsonReceived(ServerWorker *sender, const QJsonObject &docObj)
{
    const QJsonValue typeVal = docObj.value("type");
    if (typeVal.isNull() || !typeVal.isString())
        return;

    if (typeVal.toString().compare("message", Qt::CaseInsensitive) == 0) {
        // 公共消息处理（原有代码）
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

        // 广播给所有人，包括发送者自己
        broadcast(message, nullptr);

        // 记录消息到日志
        emit logMessage(QString("公共消息: %1 -> %2").arg(sender->userName()).arg(text));

    } else if (typeVal.toString().compare("private", Qt::CaseInsensitive) == 0) {
        // 私聊消息处理
        const QJsonValue textVal = docObj.value("text");
        const QJsonValue receiverVal = docObj.value("receiver");

        if (textVal.isNull() || !textVal.isString() ||
            receiverVal.isNull() || !receiverVal.isString())
            return;

        const QString text = textVal.toString().trimmed();
        const QString receiver = receiverVal.toString();

        if (text.isEmpty() || receiver.isEmpty())
            return;

        // 查找接收者
        ServerWorker *receiverWorker = nullptr;
        for (ServerWorker *worker : m_clients) {
            if (worker->userName() == receiver) {
                receiverWorker = worker;
                break;
            }
        }

        if (!receiverWorker) {
            // 接收者不在线
            QJsonObject errorMsg;
            errorMsg["type"] = "error";
            errorMsg["text"] = QString("用户 %1 不在线").arg(receiver);
            sender->sendJson(errorMsg);
            return;
        }

        if (receiverWorker == sender) {
            // 不能给自己发私聊
            QJsonObject errorMsg;
            errorMsg["type"] = "error";
            errorMsg["text"] = "不能给自己发私聊消息";
            sender->sendJson(errorMsg);
            return;
        }

        // 构建私聊消息
        QJsonObject privateMessage = docObj;
        // 确保发送者信息正确
        privateMessage["sender"] = sender->userName();
        // 确保有时间戳
        if (!privateMessage.contains("timestamp")) {
            privateMessage["timestamp"] = QDateTime::currentDateTime().toString("hh:mm:ss");
        }

        // 发送给接收者
        receiverWorker->sendJson(privateMessage);

        // 同时发送给发送者（让发送者也能看到自己发的消息）
        sender->sendJson(privateMessage);

        // 记录日志
        emit logMessage(QString("私聊消息: %1 -> %2 : %3")
                            .arg(sender->userName())
                            .arg(receiver)
                            .arg(text));

    } else if (typeVal.toString().compare("login", Qt::CaseInsensitive) == 0) {
        // 登录处理（原有代码）
        const QJsonValue usernameVal = docObj.value("text");
        if (usernameVal.isNull() || !usernameVal.isString())
            return;

        sender->setUserName(usernameVal.toString());
        QJsonObject connectedMessage;
        connectedMessage["type"] = "newuser";
        connectedMessage["username"] = sender->userName();

        // 广播给所有人，包括新登录用户自己
        broadcast(connectedMessage, sender);

        //send user list to new logined user
        QJsonObject userListMessage;
        userListMessage["type"] = "userlist";
        QJsonArray userlist;
        for (ServerWorker *worker : m_clients) {
            userlist.append(worker->userName());  // 所有用户都一样显示
        }
        userListMessage["userlist"] = userlist;
        sender->sendJson(userListMessage);

        emit logMessage(QString("用户登录: %1 (在线用户: %2)").arg(sender->userName()).arg(m_clients.size()));
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
