#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHostAddress>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QListWidgetItem>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->stackedWidget->setCurrentWidget(ui->loginPage);
    m_chatClient = new ChatClient(this);
    m_privateChatTarget = "";  // 初始化私聊对象为空

    connect(m_chatClient, &ChatClient::connected, this, &MainWindow::connectedToServer);
    connect(m_chatClient, &ChatClient::jsonReceived, this, &MainWindow::jsonReceived);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_loginButton_clicked()
{
    m_chatClient->connectToServer(QHostAddress(ui->serverEdit->text()), 1967);
}

void MainWindow::on_sayButton_clicked()
{
    if (!ui->sayLineEdit->text().isEmpty()) {
        m_chatClient->sendMessage(ui->sayLineEdit->text());
        ui->sayLineEdit->clear();
    }
}

void MainWindow::on_logoutButton_clicked()
{
    m_chatClient->disconnectFromHost();
    ui->stackedWidget->setCurrentWidget(ui->loginPage);

    for ( auto aItem : ui->userListWidget->findItems(ui->usernameEdit->text(), Qt::MatchExactly) ) {
        ui->userListWidget->removeItemWidget(aItem);
        delete aItem;
    }
}

// ==================== 私聊功能实现 ====================

void MainWindow::on_privateChatButton_clicked()
{
    // 切换到私聊页面
    ui->stackedWidget->setCurrentWidget(ui->privateChatPage);

    // 更新私聊页面标题
    ui->privateChatLabel->setText("私聊界面 - 请从右侧列表中选择用户");

    // 清空之前的私聊记录
    ui->privateTextEdit->clear();
    ui->privateUserListWidget->clear();

    // 复制公共聊天室的用户列表到私聊页面
    for (int i = 0; i < ui->userListWidget->count(); i++) {
        QListWidgetItem *item = ui->userListWidget->item(i);
        QString userName = item->text();

        // 不显示自己
        if (userName != m_currentUserName) {
            ui->privateUserListWidget->addItem(userName);
        }
    }

    // 添加提示消息
    if (ui->privateUserListWidget->count() == 0) {
        ui->privateTextEdit->append("当前没有其他在线用户");
    }
}

void MainWindow::on_backButton_clicked()
{
    // 返回公共聊天页面
    ui->stackedWidget->setCurrentWidget(ui->chatPage);
    m_privateChatTarget = "";  // 清空私聊对象
}

void MainWindow::on_privateSendButton_clicked()
{
    QString text = ui->privateSayLineEdit->text().trimmed();
    if (text.isEmpty() || m_privateChatTarget.isEmpty()) {
        if (m_privateChatTarget.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先选择私聊对象");
        }
        return;
    }

    // 构建私聊消息JSON
    QJsonObject privateMessage;
    privateMessage["type"] = "private";
    privateMessage["text"] = text;
    privateMessage["receiver"] = m_privateChatTarget;

    // 这里需要修改chatclient.cpp来支持发送JSON对象
    // 临时方案：使用sendMessage发送JSON字符串
    QJsonDocument doc(privateMessage);
    QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    // 发送私聊消息
    m_chatClient->sendMessage(jsonString, "json");

    // 在本地显示自己发送的私聊消息
    QString message = QString("[私聊] 我对 %1 说: %2").arg(m_privateChatTarget).arg(text);
    ui->privateTextEdit->append(message);

    // 清空输入框
    ui->privateSayLineEdit->clear();
}

void MainWindow::connectedToServer()
{
    m_currentUserName = ui->usernameEdit->text();  // 保存当前用户名
    ui->stackedWidget->setCurrentWidget(ui->chatPage);
    m_chatClient->sendMessage(ui->usernameEdit->text(), "login");
}

void MainWindow::messageReceived(const QString &sender, const QString &text)
{
    ui->roomTextEdit->append(QString("%1 : %2").arg(sender).arg(text));
}

void MainWindow::jsonReceived(const QJsonObject &docObj)
{
    const QJsonValue typeVal = docObj.value("type");
    if (typeVal.isNull() || !typeVal.isString())
        return;

    if (typeVal.toString().compare("message", Qt::CaseInsensitive) == 0) {
        const QJsonValue textVal = docObj.value("text");
        const QJsonValue senderVal = docObj.value("sender");

        if (textVal.isNull() || !textVal.isString())
            return;

        messageReceived(senderVal.toString(), textVal.toString());

    } else if (typeVal.toString().compare("newuser", Qt::CaseInsensitive) == 0) {
        const QJsonValue usernameVal = docObj.value("username");
        if (usernameVal.isNull() || !usernameVal.isString())
            return;

        userJoined(usernameVal.toString());
    } else if (typeVal.toString().compare("userdisconnected", Qt::CaseInsensitive) == 0) {
        const QJsonValue usernameVal = docObj.value("username");
        if (usernameVal.isNull() || !usernameVal.isString())
            return;
        userLeft(usernameVal.toString());
    } else if (typeVal.toString().compare("userlist", Qt::CaseInsensitive) == 0) {
        const QJsonValue userlistVal = docObj.value(QLatin1String("userlist"));
        if (userlistVal.isNull() || !userlistVal.isArray())
            return;

        qDebug() << userlistVal.toVariant().toStringList();
        userlistReceived(userlistVal.toVariant().toStringList());

        // 更新私聊页面的用户列表（如果当前在私聊页面）
        if (ui->stackedWidget->currentWidget() == ui->privateChatPage) {
            ui->privateUserListWidget->clear();
            QStringList userList = userlistVal.toVariant().toStringList();
            for (const QString &user : userList) {
                if (user != m_currentUserName) {
                    ui->privateUserListWidget->addItem(user);
                }
            }
        }
    } else if (typeVal.toString().compare("private", Qt::CaseInsensitive) == 0) {
        // 处理私聊消息
        const QJsonValue textVal = docObj.value("text");
        const QJsonValue senderVal = docObj.value("sender");
        const QJsonValue receiverVal = docObj.value("receiver");

        if (textVal.isString() && senderVal.isString() && receiverVal.isString()) {
            QString sender = senderVal.toString();
            QString receiver = receiverVal.toString();
            QString text = textVal.toString();

            // 如果这个消息是发给我的
            if (receiver == m_currentUserName) {
                // 如果当前在私聊页面且正在和发送者聊天，显示消息
                if (ui->stackedWidget->currentWidget() == ui->privateChatPage &&
                    m_privateChatTarget == sender) {
                    QString message = QString("[私聊] %1 对我说: %2").arg(sender).arg(text);
                    ui->privateTextEdit->append(message);
                } else {
                    // 否则给出提示
                    QMessageBox::information(this, "新私聊消息",
                                             QString("收到来自 %1 的私聊消息:\n%2").arg(sender).arg(text));
                }
            }
        }
    }
}

void MainWindow::userJoined(const QString &user)
{
    ui->userListWidget->addItem(user);
}

void MainWindow::userLeft(const QString &user)
{
    for ( auto aItem : ui->userListWidget->findItems(user, Qt::MatchExactly) ) {
        ui->userListWidget->removeItemWidget(aItem);
        delete aItem;
    }
}

void MainWindow::userlistReceived(const QStringList &list)
{
    ui->userListWidget->clear();
    ui->userListWidget->addItems(list);
}
