#include <QGuiApplication>
#include <QQuickStyle>
#include <QQmlApplicationEngine>

#include "global.h"
#include <QWKQuick/quickwindowagent.h>
#include "Http_Layer/httpmgr.h"
#include "Logic_Layer/logicmgr.h"
#include "Net_Layer/tcpmgr.h"
#include "Sqlite_Layer/sqlitemgr.h"
#include "UserData_Layer/usermgr.h"
#include "ChatMsg_Layer/chatmgr.h"

/******************************************************************************
 *
 * @file       main.cpp
 * @brief      主程序入口
 *
 * @author     YZC
 * @date       2026/03/02
 * @history
 *****************************************************************************/

// iniConfig
void iniConfig() {
    // 预处理配置文件
    QString fileName = "config.ini";
    QString appPath = QCoreApplication::applicationDirPath();
    QString configPath = QDir::toNativeSeparators(appPath + QDir::separator() + fileName);
    QSettings settings(configPath, QSettings::IniFormat);
    QString gateHost = settings.value("GateServer/host").toString();
    QString gatePort = settings.value("GateServer/port").toString();
    gate_url_prefix = "https://" + gateHost + ":" + gatePort;
    qDebug() << gate_url_prefix;
}

// 初始化单例(保证线程亲和性安全，必须饿汉)
void initSingleton() {
    qDebug() << "1. 准备初始化 HttpMgr...";
    auto& HttpInstance = HttpMgr::getInstance();

    qDebug() << "2. 准备初始化 LogicMgr...";
    auto& LogicInstance = LogicMgr::getInstance();

    qDebug() << "3. 准备初始化 TcpMgr...";
    auto& TcpMgrInstance = TcpMgr::getInstance();

    qDebug() << "4. 准备初始化 SqliteMgr...";
    auto& SqlInstance = SqliteMgr::getInstance();

    qDebug() << "5. 准备初始化 UserMgr...";
    auto& UserInstance = UserMgr::getInstance();

    qDebug() << "6. 准备初始化 ChatMgr...";
    auto& ChatInstance = ChatMgr::getInstance();

    qDebug() << "7. 所有单例初始化完毕！";

    qDebug() << "a.HttpInstance init...";
    HttpInstance.init();

    qDebug() << "b.LogicInstance init...";
    LogicInstance.init();

    qDebug() << "c.TcpMgrInstance init...";
    TcpMgrInstance.init();

    qDebug() << "d.SqlInstance init...";
    SqlInstance.init();

    qDebug() << "e.ChatInstance init...";
    ChatInstance.init();

    qDebug() << "f.UserInstance init...";
    UserInstance.init();
}

void initRegisterTypes() {
    // 在 main.cpp 的最开头注册：
    qRegisterMetaType<ReqId>("ReqId");
    qRegisterMetaType<MsgInfo>("MsgInfo");
    qRegisterMetaType<ConversationInfo>("ConversationInfo");
    qRegisterMetaType<FriendApplyInfo>("FriendApplyInfo");

    qRegisterMetaType<QList<MsgInfo>>("QList<MsgInfo>");
    qRegisterMetaType<QList<ConversationInfo>>("QList<ConversationInfo>");
    qRegisterMetaType<QList<std::shared_ptr<UserInfo>>>("QList<std::shared_ptr<UserInfo>>");
    qRegisterMetaType<QList<std::shared_ptr<ConversationInfo>>>("QList<std::shared_ptr<ConversationInfo>>");
    qRegisterMetaType<QList<std::shared_ptr<FriendApplyInfo>>>("QList<std::shared_ptr<FriendApplyInfo>>");
    qRegisterMetaType<QHash<QString, UserInfo>>("QHash<QString, UserInfo>");
    qRegisterMetaType<QList<FriendApplyInfo>>("QList<FriendApplyInfo>");

    qRegisterMetaType<std::shared_ptr<MsgInfo>>("std::shared_ptr<MsgInfo>");
    qRegisterMetaType<std::shared_ptr<UserInfo>>("std::shared_ptr<UserInfo>");
    qRegisterMetaType<std::shared_ptr<ConversationInfo>>("std::shared_ptr<ConversationInfo>");
    qRegisterMetaType<std::shared_ptr<FriendApplyInfo>>("std::shared_ptr<FriendApplyInfo>");
}

int main(int argc, char *argv[]) {
    // 设置风格
    QQuickStyle::setStyle("Fusion");
    // 创建应用程序，底层事件循环
    QGuiApplication app(argc, argv);
    // 设置应用程序图标
    app.setWindowIcon(QIcon(":/cyberpunk_skull_32x32.ico"));
    // 初始化配置
    iniConfig();

    initSingleton();
    initRegisterTypes();
    // 创建QML引擎
    QQmlApplicationEngine engine;
    // 注册QWindowKit
    QWK::registerTypes(&engine);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // 加载初始页面
    engine.load(QUrl(QStringLiteral("qrc:/Src/qml/main/Main.qml")));

    // 启动事件循环
    return app.exec();
}
