#include "threadpool.h"
#include "serverworker.h"
#include <QMetaObject>
#include <QThread>

void MessageTask::run()
{
    if (m_receiver) {
        // 模拟一些处理时间（实际使用时可以根据消息复杂度调整）
        QThread::msleep(1);

        QMetaObject::invokeMethod(m_receiver, "broadcastMessage",
                                  Qt::QueuedConnection,
                                  Q_ARG(QJsonObject, m_message),
                                  Q_ARG(ServerWorker*, m_exclude));
    }
}

void ConnectionTask::run()
{
    if (m_receiver) {
        // 模拟连接处理时间
        QThread::msleep(10);

        QMetaObject::invokeMethod(m_receiver, "handleNewConnection",
                                  Qt::QueuedConnection,
                                  Q_ARG(qintptr, m_socketDescriptor));
    }
}

ThreadPoolManager::ThreadPoolManager(QObject *parent)
    : QObject(parent)
{
    // 设置线程池大小为10，可以根据需要调整
    m_threadPool.setMaxThreadCount(10);
    m_threadPool.setExpiryTimeout(30000); // 30秒后回收空闲线程
}

void ThreadPoolManager::startMessageTask(QObject* receiver, const QJsonObject &message, ServerWorker *exclude)
{
    MessageTask *task = new MessageTask(receiver, message, exclude);
    task->setAutoDelete(true);
    m_threadPool.start(task);
}

void ThreadPoolManager::startConnectionTask(QObject* receiver, qintptr socketDescriptor)
{
    ConnectionTask *task = new ConnectionTask(receiver, socketDescriptor);
    task->setAutoDelete(true);
    m_threadPool.start(task);
}

int ThreadPoolManager::activeThreadCount() const
{
    return m_threadPool.activeThreadCount();
}
