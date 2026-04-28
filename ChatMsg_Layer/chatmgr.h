#ifndef CHATMGR_H
#define CHATMGR_H

#include "ChatMsg_Layer/chatsession.h"
#include "ChatMsg_Layer/sendratelimiter.h"
#include "Net_Layer/tcpmgr.h"
#include <QObject>

enum class SyncState {
    Waiting,    // 在后台排队中，还未发出网络请求
    Fetching    // 正在向服务器请求中（等待回包）
};

struct SyncTask {
    SyncState state;
    qint64 lastReqTime; // 发起请求的时间戳，用于超时判定
    quint64 startVersion;  // 本地的老游标
    quint64 endVersion;    // 动态变化的右边界
};

struct PendingMsgNode {
    QString clientMsgId;
    QString targetUid;
    int sessionType;           // 用于数据库更新
    qint64 createTime;         // 用于断网重连时的绝对时序排序
    QByteArray networkPayload; // 直接存组装好的 JSON 字节流，重传时直接发，省去反复组装
    int retryCount = 0;
    QTimer* timer = nullptr;   // 专属定时器
};

class ChatMgr: public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // 暴露给 QML 使用
    Q_PROPERTY(ConversationsModel* convModel READ getConvModel CONSTANT)
    Q_PROPERTY(QString currentActiveSessionKey READ getCurrentActiveSessionKey CONSTANT)
    Q_PROPERTY(int netState READ getNetState NOTIFY netStateChanged)

public:
    void init();

    // 单例创建接口
    static ChatMgr& getInstance();
    static ChatMgr* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    ChatSession* getChatSession(const QString& targetUid, int sessionType);

    ConversationsModel* getConvModel() const;

    QString getCurrentActiveSessionKey() const;

    int getNetState() const;

    void setConversationList(const QList<ConversationInfo>& convList);

    void clearConvList();

    void clear();

    void addSyncTask(const QString& peerUid, int sessionType, quint64 localVersion, quint64 endVersion);

    void completeSyncTask(const QString& peerUid, int sessionType);

    bool isSessionSyncing(const QString& peerUid, int sessionType) const;

    void processNextSyncTask();

    void handleSyncMsgResult(const QString& peerUid, int sessionType, const QList<MsgInfo>& newMsgs, bool hasMore);

    void checkAndStartSyncTasks();

    Q_INVOKABLE ChatMsgModel* getChatModel(const QString& targetUid, int sessionType);

    Q_INVOKABLE void clearAllSessions();

    Q_INVOKABLE void setCurrentActiveSession(const QString& uid, int sessionType);

    Q_INVOKABLE void sendChatMessage(const QString& targetUid, int sessionType, const QString& msgData);

    Q_INVOKABLE void clearUnread(const QString& peerUid, int sessionType);

signals:
    void netStateChanged();

    void sig_sync_history_finished();

public slots:
    void slot_on_receive_chat_msg(const MsgInfo& msgInfo);

    void slot_recv_msg_rsp(std::shared_ptr<MsgInfo> msgInfo, std::shared_ptr<ConversationInfo> convInfo);

    void slot_net_state_changed(NetState state);

private:
    ChatMgr(QObject* parent = nullptr);

    QString formatSessionKey(const QString& uid, int type) const {
        return uid + "_" + QString::number(type);
    }

    void makeRecent(const QString& sessionKey); // LRU 提权

    void markAsRead(const QString& peerUid, int sessionType);

    void loadPendingMessages();

    void shutDownAllFlyingMsgs();

    void clearFlyingMsgs();

    void sendPendingMsgNodeList(QList<PendingMsgNode*> list);

    QByteArray prepareMsgInfoJsonStr(const MsgInfo& msgInfo);

    PendingMsgNode* createPendingMsgNode(const MsgInfo& msgInfo, QByteArray jsonData);
private:
    QString _currentActiveSessionKey; // 当前用户正在看的会话 UID

    // 所有交流过的会话状态 (轻量级，常驻内存，不走LRU)
    QHash<QString, ChatSession*> _sessions;

    ConversationsModel* _convModel;

    // LRU 数据结构：只记录存活了 ChatMsgModel 的会话
    constexpr static int MAX_SESSION_CACHE = 15; // 最大同时存在 15 个聊天会话内存
    std::list<QString> _lruQueue;     // 链表头部为最近使用，尾部为最久未使用
    QHash<QString, std::list<QString>::iterator> _lruIters; // O(1) 定位链表节点

    // 消息重发缓存（这里不采用queue是应对复杂的网络环境，而不是过度依赖Tcp与服务器Strand）
    QHash<QString, PendingMsgNode*> _flyingMsgs;

    // 发送阈值
    SendRateLimiter _rateLimiter;

    // 网络状态
    NetState _netState = NetState::Disconnected;

    // 同步任务映射表
    QHash<QString, SyncTask> _syncTasks; // TODO: 这里采用基础的QHash与基础插队策略，没有更复杂的优先级排列，待优化
};
#endif // CHATMGR_H
