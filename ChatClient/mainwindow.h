#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "chatclient.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_loginButton_clicked();
    void on_sayButton_clicked();
    void on_logoutButton_clicked();

    // 新增的三个按钮槽函数
    void on_privateChatButton_clicked();
    void on_backButton_clicked();
    void on_privateSendButton_clicked();

    // 已有的函数
    void connectedToServer();
    void messageReceived(const QString &sender, const QString &text);
    void jsonReceived(const QJsonObject &docObj);
    void userJoined(const QString &user);
    void userLeft(const QString &user);
    void userlistReceived(const QStringList &list);

private:
    Ui::MainWindow *ui;
    ChatClient *m_chatClient;
    QString m_currentUserName;        // 当前登录用户名
    QString m_privateChatTarget;      // 私聊对象
};
#endif // MAINWINDOW_H
