#ifndef SQLITEMGR_H
#define SQLITEMGR_H
///
///
/// 因为QSqlDatabase的使用必须是在创建线程里，而且单线程调用还能避免并发问题，SqlLite写操作直接触发表锁，使用序列写入更好
///
///
#include "sqlworker.h"

class SqliteMgr : public QObject
{
    Q_OBJECT
public:
    static SqliteMgr& getInstance();

    SqliteMgr(const SqliteMgr&) = delete;

    SqliteMgr& operator=(const SqliteMgr&) = delete;

    void init();

    void initDatabase(const QString& uid);

    void closeDatabase();

    /// 用户信息缓存表 (user_info_local)
    // 插入或更新单个用户的缓存资料 (好友、群友、陌生人都在这)
    void addOrUpdateUserInfo(const QString& uid, const QString& username, const QString& email, const QString& avatarUrl = "");

    void reqUserInfo(const QString& uid);

    /// 好友关系表 (friend_list_local)
    // 添加或更新好友信息 (SQLite 特有的 UPSERT 语法)
    void addOrUpdateFriend(std::shared_ptr<UserInfo> userInfo);

    // 删除好友
    void deleteFriend(const QString& friendUid);

    // 获取本地所有好友(联表查询 user_info_local，秒开界面）
    void getAllFriends();

    /// 会话列表表 (conversation_local)
    void appendMessageTransaction(const QList<MsgInfo>& msgList, const ConversationInfo& convInfo);

    void getSeq(const QString& peerUid, int sessionType);

    // 更新expected_recv_seq字段
    void updateConvExpectedRecvSeq(const QString& peerUid, int sessionType, quint64 expectRecvSeq);

    // 清零未读数
    void clearUnreadCount(const QString& peerUid, int sessionType);

    // 删除某个会话（不在列表显示了，但聊天记录还在）
    void deleteConversation(const QString& peerUid, int sessionType);

    // 获取本地所以会话信息
    void getAllConversations();

    /// 聊天记录表 (chat_history_local)
    // 追加一条聊天记录
    void addChatHistory(const MsgInfo& msg);

    // 收到服务端 ACK 后，通过 client_msg_id 更新 msg_id 和 消息发送：成功状态
    void updateMsgStatusAndVersion(std::shared_ptr<MsgInfo> msgInfo, std::shared_ptr<ConversationInfo> convInfo);

    void updateMsgReadStatus(const QString &msgId, int isRead);

    void updateAllMsgReadStatus(const QString& peerUid, int sessionType);

    void insertGapMessages(const QString& peerUid, int sessionType, QList<MsgInfo> gapMsgs, bool hasMore);

    // 分页获取历史记录（极其重要：limit是每次拉取几条）
    // oldestTime 传 0 表示获取最新的一页
    void getChatHistory(const QString& peerUid, int sessionType, qint64 oldestTime = 0, int limit = 20);

    // 自聊接口
    void updateSelfChatReceiveInfo(const MsgInfo& msgInfo);

    // 调用内部SqlWorker对象的INVOKABLE 方法(均同步)
    quint64 getMaxFriendVersion();

    quint64 getMaxConversationVersion();

    quint64 getMaxFriendApplyVersion();

    QList<std::shared_ptr<UserInfo>> getAllFriendsSync();

    QList<ConversationInfo> getAllConversationsSync();

    QList<FriendApplyInfo> getAllFriendAppliesSync();

    void syncDeltaFriends(const QList<std::shared_ptr<UserInfo>>& deltaUserList);

    void syncDeltaConversations(const QList<ConversationInfo>& deltaConvList);

    bool appendMessageTransactionSync(const QList<MsgInfo>& msgList, const ConversationInfo& convInfo);

    bool addOrUpdateUserInfoSync(std::shared_ptr<UserInfo>);

    bool insertGapMessagesSync(const QString& peerUid, int sessionType, QList<MsgInfo> gapMsgs);

    QList<MsgInfo> getPendingMessagesSync();

    bool addOrUpdateFriendSync(std::shared_ptr<UserInfo> userInfo);

    quint64 getLocalMaxVersionSync(const QString& peerUid, int sessionType);

    bool upsertFriendApplySync(std::shared_ptr<FriendApplyInfo> applyInfo);

    bool updateAuthFriendSync(const QString& fromUid, const QString& toUid, int status, quint64 handleTime, quint64 version);

    bool updateFriendApplyVersionSync(const QString& fromUid, const QString& toUid, quint64 version);

    bool updateFriendListVersionSync(const QString& friendUid, quint64 version);

private:
    explicit SqliteMgr(QObject *parent = nullptr);

    ~SqliteMgr();

    QThread* _thread{};

    SqlWorker* _sqlWorker{};

signals:
    // 新增内部派发信号
    void sig_initDb(const QString& uid);

    void sig_closeDb();

    void sig_initDb_success();

    void sig_initDb_failed();

    /// 用户信息缓存表 (user_info_local)
    void sig_addOrUpdateUserInfo(const QString& uid, const QString& username, const QString& email, const QString& avatarUrl);

    void sig_reqUserInfo(const QString& uid);

    void sig_rspUserInfo(std::shared_ptr<UserInfo> userInfo);

    /// 好友关系表 (friend_list_local)
    void sig_addOrUpdateFriend(std::shared_ptr<UserInfo> userInfo);

    void sig_deleteFriend(const QString& friendUid);

    void sig_getAllFriends();

    /// 会话列表表 (conversation_local)
    void sig_appendMessageTransaction(const QList<MsgInfo>& msgList, const ConversationInfo& convInfo);

    void sig_appendMessageTransaction_success(const QList<MsgInfo>& msgList);

    void sig_updateConvExpectedRecvSeq(const QString& peerUid, int sessionType, quint64 expectedRecvSeq);

    void sig_getSeq(const QString& peerUid, int sessionType);

    void sig_getSeq_result(const QString& peerUid, int sessionType, uint64_t nextSendSeq, uint64_t expectedRecvSeq);

    void sig_clearUnreadCount(const QString& peerUid, int sessionType);

    void sig_deleteConversation(const QString& peerUid, int sessionType);

    void sig_getAllConversations();

    /// 聊天记录表 (chat_history_local)
    void sig_addChatHistory(const MsgInfo& msg);

    void sig_updateMsgStatusAndVersion(std::shared_ptr<MsgInfo> msgInfo, std::shared_ptr<ConversationInfo> convInfo);

    void sig_updateMsgReadStatus(const QString &msgId, int isRead);

    void sig_updateAllMsgReadStatus(const QString& peerUid, int sessionType);

    void sig_getChatHistory(const QString& peerUid, int sessionType, qint64 oldestTime, int limit);

    void sig_insertGapMessages(const QString &peerUid, int sessionType, QList<MsgInfo> gapMsgs, bool hasMore);

    void sig_sync_history_received(const QString &peerUid, int sessionType, QList<MsgInfo> gapMsgs, bool hasMore);

    /// 获取好友/聊天记录列表
    void sig_getAllFriendsResult(QList<std::shared_ptr<UserInfo>> friendList);

    void sig_getChatHistoryResult(QString peerUid, int sessionType, QList<MsgInfo> historyList);

    void sig_getAllConversationsResult(QList<ConversationInfo> convList);

    /// 自聊信号
    void sig_updateSelfChatReceiveInfo(const MsgInfo& msgInfo);
};

#endif // SQLITEMGR_H
