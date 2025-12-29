#ifndef MESSAGESTORAGE_H
#define MESSAGESTORAGE_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QDir>

class MessageStorage : public QObject
{
    Q_OBJECT
public:
    explicit MessageStorage(QObject *parent = nullptr);
    ~MessageStorage();

    void initStorage(const QString &storagePath = "chat_logs");
    void savePublicMessage(const QString &sender, const QString &message);
    void savePrivateMessage(const QString &sender, const QString &receiver, const QString &message);
    QStringList getChatHistory(const QString &user1, const QString &user2 = "", int limit = 100);
    void saveLoginLog(const QString &username, const QString &ip, bool isLogin);

private:
    QString m_storagePath;
    QFile m_publicLogFile;
    QFile m_privateLogFile;
    QFile m_loginLogFile;
    QMutex m_mutex;

    void ensureDirectoryExists(const QString &path);
    QString getTodayDateString() const;
    QString formatMessage(const QString &type, const QString &sender, const QString &receiver, const QString &message);
};

#endif // MESSAGESTORAGE_H
