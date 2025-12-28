#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QObject>
#include <QTcpServer>
#include "serverworker.h"
#include "threadpool.h"

class ChatServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit ChatServer(QObject *parent = nullptr);
    ~ChatServer();

protected:
    void incomingConnection(qintptr socketDescriptor) override;
    QVector<ServerWorker*> m_clients;

    // 线程池管理器
    ThreadPoolManager* m_threadPool;

    void broadcast(const QJsonObject &message, ServerWorker *exclude);

signals:
    void logMessage(const QString &msg);
    // 用于线程池调用的信号
    void broadcastMessage(const QJsonObject &message, ServerWorker *exclude);
    void handleNewConnection(qintptr socketDescriptor);

public slots:
    void stopServer();
    void jsonReceived(ServerWorker *sender, const QJsonObject &docObj);
    void userDisconnected(ServerWorker *sender);
    // 线程池任务对应的槽函数
    void onBroadcastMessage(const QJsonObject &message, ServerWorker *exclude);
    void onHandleNewConnection(qintptr socketDescriptor);
};

#endif // CHATSERVER_H
