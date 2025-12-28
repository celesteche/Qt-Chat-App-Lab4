#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <QObject>
#include <QThreadPool>
#include <QRunnable>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>

// 前向声明，避免循环引用
class ServerWorker;

class MessageTask : public QRunnable
{
public:
    MessageTask(QObject* receiver, const QJsonObject &message, ServerWorker *exclude = nullptr)
        : m_receiver(receiver), m_message(message), m_exclude(exclude)
    {}

    void run() override;

private:
    QObject* m_receiver;
    QJsonObject m_message;
    ServerWorker* m_exclude;
};

class ConnectionTask : public QRunnable
{
public:
    ConnectionTask(QObject* receiver, qintptr socketDescriptor)
        : m_receiver(receiver), m_socketDescriptor(socketDescriptor)
    {}

    void run() override;

private:
    QObject* m_receiver;
    qintptr m_socketDescriptor;
};

class ThreadPoolManager : public QObject
{
    Q_OBJECT
public:
    explicit ThreadPoolManager(QObject *parent = nullptr);

    void startMessageTask(QObject* receiver, const QJsonObject &message, ServerWorker *exclude = nullptr);
    void startConnectionTask(QObject* receiver, qintptr socketDescriptor);
    int activeThreadCount() const;

private:
    QThreadPool m_threadPool;
};

#endif // THREADPOOL_H
