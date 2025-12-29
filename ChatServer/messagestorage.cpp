#include "messagestorage.h"
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QCoreApplication>

MessageStorage::MessageStorage(QObject *parent)
    : QObject(parent)
{
    // 默认存储路径为应用程序目录下的 chat_logs 文件夹
    QString defaultPath = QCoreApplication::applicationDirPath() + "/chat_logs";
    initStorage(defaultPath);
}

MessageStorage::~MessageStorage()
{
    if (m_publicLogFile.isOpen()) m_publicLogFile.close();
    if (m_privateLogFile.isOpen()) m_privateLogFile.close();
    if (m_loginLogFile.isOpen()) m_loginLogFile.close();
}

void MessageStorage::initStorage(const QString &storagePath)
{
    QMutexLocker locker(&m_mutex);

    m_storagePath = storagePath;
    ensureDirectoryExists(m_storagePath);

    QString dateStr = getTodayDateString();
    QString publicLogPath = m_storagePath + "/public_" + dateStr + ".log";
    QString privateLogPath = m_storagePath + "/private_" + dateStr + ".log";
    QString loginLogPath = m_storagePath + "/login_" + dateStr + ".log";

    // 打开或创建日志文件
    m_publicLogFile.setFileName(publicLogPath);
    if (!m_publicLogFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qDebug() << "无法打开公共聊天日志文件:" << publicLogPath;
    }

    m_privateLogFile.setFileName(privateLogPath);
    if (!m_privateLogFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qDebug() << "无法打开私聊日志文件:" << privateLogPath;
    }

    m_loginLogFile.setFileName(loginLogPath);
    if (!m_loginLogFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qDebug() << "无法打开登录日志文件:" << loginLogPath;
    }

    qDebug() << "消息存储初始化完成，路径:" << m_storagePath;
}

void MessageStorage::savePublicMessage(const QString &sender, const QString &message)
{
    QMutexLocker locker(&m_mutex);

    if (!m_publicLogFile.isOpen()) {
        initStorage(m_storagePath); // 重新初始化
    }

    QString formattedMsg = formatMessage("PUBLIC", sender, "ALL", message);
    QTextStream out(&m_publicLogFile);
    out << formattedMsg << "\n";
    m_publicLogFile.flush();

    qDebug() << "保存公共消息:" << formattedMsg;
}

void MessageStorage::savePrivateMessage(const QString &sender, const QString &receiver, const QString &message)
{
    QMutexLocker locker(&m_mutex);

    if (!m_privateLogFile.isOpen()) {
        initStorage(m_storagePath); // 重新初始化
    }

    QString formattedMsg = formatMessage("PRIVATE", sender, receiver, message);
    QTextStream out(&m_privateLogFile);
    out << formattedMsg << "\n";
    m_privateLogFile.flush();

    qDebug() << "保存私聊消息:" << formattedMsg;
}

void MessageStorage::saveLoginLog(const QString &username, const QString &ip, bool isLogin)
{
    QMutexLocker locker(&m_mutex);

    if (!m_loginLogFile.isOpen()) {
        initStorage(m_storagePath);
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString action = isLogin ? "LOGIN" : "LOGOUT";
    QString logEntry = QString("[%1] %2 %3 from %4")
                           .arg(timestamp)
                           .arg(action)
                           .arg(username)
                           .arg(ip);

    QTextStream out(&m_loginLogFile);
    out << logEntry << "\n";
    m_loginLogFile.flush();

    qDebug() << "保存登录日志:" << logEntry;
}

QStringList MessageStorage::getChatHistory(const QString &user1, const QString &user2, int limit)
{
    QMutexLocker locker(&m_mutex);
    QStringList history;

    QString filename;
    if (user2.isEmpty()) {
        // 获取公共聊天历史
        QString dateStr = getTodayDateString();
        filename = m_storagePath + "/public_" + dateStr + ".log";
    } else {
        // 获取私聊历史
        QString dateStr = getTodayDateString();
        filename = m_storagePath + "/private_" + dateStr + ".log";
    }

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法读取聊天历史文件:" << filename;
        return history;
    }

    QTextStream in(&file);
    int count = 0;
    while (!in.atEnd() && count < limit) {
        QString line = in.readLine();
        if (user2.isEmpty()) {
            // 公共聊天历史，全部返回
            history.prepend(line); // 最新的在前面
            count++;
        } else {
            // 私聊历史，只返回两个用户之间的
            if (line.contains(user1) && line.contains(user2)) {
                history.prepend(line);
                count++;
            }
        }
    }

    file.close();
    return history;
}

void MessageStorage::ensureDirectoryExists(const QString &path)
{
    QDir dir;
    if (!dir.exists(path)) {
        dir.mkpath(path);
        qDebug() << "创建目录:" << path;
    }
}

QString MessageStorage::getTodayDateString() const
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd");
}

QString MessageStorage::formatMessage(const QString &type, const QString &sender,
                                      const QString &receiver, const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    if (type == "PUBLIC") {
        return QString("[%1][PUBLIC][%2] %3")
        .arg(timestamp)
            .arg(sender)
            .arg(message);
    } else {
        return QString("[%1][PRIVATE][%2->%3] %4")
        .arg(timestamp)
            .arg(sender)
            .arg(receiver)
            .arg(message);
    }
}
