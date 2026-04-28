#ifndef GLOBAL_H
#define GLOBAL_H

// Qt头文件
#include <QString>
#include <QIcon>
#include <QDir>
#include <QSettings>
#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QtQml/qqmlregistration.h>
#include <QByteArray>
#include <QNetworkReply>
#include <QMap>
#include <QTcpSocket>
#include <QSslSocket>
#include <QQmlEngine> // 引入 QML 引擎头文件
#include <QJSEngine>
#include <QHash>
#include <QThread>
#include <QTimer>
#include <QQueue>
#include <QPair>

// 标准库头文件
#include <functional>

extern QString gate_url_prefix;

extern std::function<QString(QString)> xorString;

QString getClientMsgId();

struct ServerInfo {
    uint64_t uid;
    QString host;
    QString port;
    QString token;
};

enum ReqId
{
    ID_GET_VARIFY_CODE = 1001,  // 获取验证码
    ID_REG_USER = 1002, // 注册用户
    ID_RESET_PWD = 1003, // 重置密码
    ID_LOGIN_USER = 1004, // 用户登录
    ID_CHAT_LOGIN = 1005, // 登录聊天服务器
    ID_CHAT_LOGIN_RSP = 1006, // 登录聊天服务器回包
    ID_CHAT_SEND = 1007,  // 客户端发送消息
    ID_CHAT_RECEIVE = 1008, // 客户端接收消息
    ID_SEARCH_USER_REQ = 1009, // 客户端查询用户
    ID_SEARCH_USER_RSP = 1010, // 客户端查询回包
    ID_ADD_FRIEND_REQ = 1011, // 客户端添加好友请求
    ID_ADD_FRIEND_RSP = 1012, // 客户端添加好友响应
    ID_ADD_FRIEND_NOTIFY = 1013, // 服务器端通知客户端
    ID_AUTH_FRIEND_REQ = 1014, // 客户端同意或拒绝请求，并通知服务器
    ID_AUTH_FRIEND_RSP = 1015, // 客户端同意或拒绝响应
    ID_AUTH_FRIEND_NOTIFY = 1016, // 服务器携带Target客户端状态通知源客户端
    ID_GET_FRIEND_LIST_REQ = 1017, // 客户端拉取好友列表的请求
    ID_GET_FRIEND_LIST_RSP = 1018,   // 客户端拉取好友列表的响应
    ID_CHAT_MSG_REQ = 1019,   // 客户端发送单聊文本消息
    ID_CHAT_MSG_RSP = 1020,   // 服务器回执（如果需要处理失败重发，暂保留）
    ID_CHAT_MSG_NOTIFY = 1021, // 服务器下发单聊文本消息给接收方
    ID_CHAT_MSG_NOTIFY_ACK = 1022,
    ID_HEARTBEAT_REQ = 1023, // 客户端心跳请求
    ID_HEARTBEAT_RSP = 1024, // 客户端心跳响应
    ID_SYNC_INIT_REQ = 1025, // 初始化同步请求
    ID_SYNC_INIT_RSP = 1026, // 初始化同步响应
    ID_CHAT_MSG_READ_REQ = 1027, // 客户端已读请求
    ID_CHAT_MSG_READ_RSP = 1028, // 客户端已读响应
    ID_CHAT_MSG_READ_NOTIFY = 1029, // 服务器已读通知
    ID_SYNC_MSG_REQ = 1030, // 客户端拉取历史记录请求
    ID_SYNC_MSG_RSP = 1031, // 客户端拉取历史记录响应
    ID_MSG_REPAIR_REQ = 1032, // 客户端补洞请求
    ID_MSG_REPAIR_RSP = 1033, // 客户端修补消息响应
    ID_SEQ_RESET_CMD = 1034,  // 服务端强制重置序列号指令
};

enum Modules{
    REGISTERMOD = 0, // 注册模块
    RESETMOD = 1, // 重置密码模块
    LOGINMOD = 2, // 登录模块
};

enum ErrorCodes {
    SUCCESS = 0,
    ERR_JSON = 1, // json解析失败
    ERR_NETWORK = 2, // 网络错误
    VarifyExpired = 1003, // 验证码过期
    VarifyCodeError = 1004, // 验证码错误
    UserExited = 1005, // 用户已经存在
    PasswdError = 1006, // 密码错误
    EmailNotMatch = 1007, // 邮箱不匹配
    PasswdUpFailed = 1008, // 密码更新失败
    LoginError = 1009, // 登录用户名或者密码错误
    UidInvalid = 1010, // 非法uid
    TokenInvalid = 1011, // 非法token
    UserNotFound = 1012, // 未找到用户
    UserOffline = 1013 // 用户断线
};

struct UserInfo {
    QString _uid;
    QString _username;
    QString _email;
    QString _remark;
    QString _avatarUrl;
    quint64 _version = 0; // 版本号
    quint64 _updateTime = 0;
    quint64 _createTime = 0;
    bool _isFriend;
    int _status = 1;  // 0：拉黑或删除，1：正常
};
Q_DECLARE_METATYPE(UserInfo)
Q_DECLARE_METATYPE(std::shared_ptr<UserInfo>)
Q_DECLARE_METATYPE(QList<std::shared_ptr<UserInfo>>)

struct MsgInfo {
    // === 身份与排序标志 ===
    QString clientMsgId; // 客户端生成的 UUID (本地数据库绝对主键)
    QString msgId;       // 服务端生成的雪花 ID (发送成功或接收时才有)
    uint64_t seqId = 1;  // 消息序列号 (TCP 滑动窗口防乱序核心)
    quint64 createTime = 0; // 消息产生的时间戳 (毫秒级，分页拉取的核心游标)

    // === 路由与归属 ===
    QString peerUid;     // 对端 UID (当前聊天窗口的对象)
    int sessionType = 1; // 1-单聊, 2-群聊 (方便后续扩展)
    QString senderUid;   // 真正的发送者 UID

    // === 内容与状态 ===
    QString msgData;     // 消息内容
    int msgType = 1;     // 1-文本, 2-图片等
    bool isSelf = false; // 判断是否自身所发 (UI 层可以直接用，也可以通过 senderUid == current_uid 判断)
    int sendStatus = 1;  // 发送状态：0-发送中, 1-成功, 2-失败
    int isRead = 0;      // 0：未读， 1：已读
};
Q_DECLARE_METATYPE(MsgInfo)
Q_DECLARE_METATYPE(QList<MsgInfo>)
Q_DECLARE_METATYPE(std::shared_ptr<MsgInfo>)

struct ConversationInfo {
    QString peerUid;        // 对端 UID
    int sessionType = 1;    // 1-单聊，2-群聊
    QString lastMsgSenderUid;
    int lastMsgType;
    QString lastMsgContent; // 最后一条消息摘要
    int unreadCount = 0;    // 未读消息数
    quint64 lastSeqId = 1;      // 会话全局最新（最后）的消息序列ID
    qint64 updateTime = 0;  // 最后活跃时间
    quint64 nextSendSeq = 1;
    quint64 expectedRecvSeq = 1;
    bool isPinned = false;  // 是否置顶
    bool isMuted = false;   // 是否免打扰
    quint64 version = 0; // 版本号
};
Q_DECLARE_METATYPE(ConversationInfo)
Q_DECLARE_METATYPE(QList<ConversationInfo>)
Q_DECLARE_METATYPE(std::shared_ptr<ConversationInfo>)
Q_DECLARE_METATYPE(QList<std::shared_ptr<ConversationInfo>>)

struct FriendApplyInfo {
    QString fromUid;
    QString toUid;
    int status = 0;
    QString greeting;
    quint64 applyTime = 0;
    quint64 handleTime = 0;
    quint64 version = 0;
};

Q_DECLARE_METATYPE(FriendApplyInfo)
Q_DECLARE_METATYPE(QList<FriendApplyInfo>)
Q_DECLARE_METATYPE(std::shared_ptr<FriendApplyInfo>)
Q_DECLARE_METATYPE(QList<std::shared_ptr<FriendApplyInfo>>)
#endif // GLOBAL_H
