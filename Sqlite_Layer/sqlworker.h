#ifndef SQLWORKER_H
#define SQLWORKER_H
#include "global.h"
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>

class SqlWorker : public QObject
{
    Q_OBJECT
public:
    explicit SqlWorker(QObject *parent = nullptr);

    ~SqlWorker();

public slots:
    void slot_init(const QString& uid);

    void slot_close();

    /// 用户信息缓存表 (user_info_local)
    // 插入或更新单个用户的缓存资料 (好友、群友、陌生人都在这)
    void slot_addOrUpdateUserInfo(const QString& uid, const QString& username, const QString& email, const QString& avatarUrl);

    void slot_reqUserInfo(const QString& uid);

    /// 好友关系表 (friend_list_local)
    // 添加或更新好友信息 (SQLite 特有的 UPSERT 语法)
    void slot_addOrUpdateFriend(std::shared_ptr<UserInfo> userInfo);

    // 删除好友
    void slot_deleteFriend(const QString& friendUid);

    // 获取本地所有好友(联表查询 user_info_local，秒开界面）
    void slot_getAllFriends();

    /// 会话列表表 (conversation_local)
    void slot_appendMessageTransaction(const QList<MsgInfo>& msgList, const ConversationInfo& convInfo);

    void slot_getSeq(const QString& peerUid, int sessionType);

    void slot_updateConvExpectedRecvSeq(const QString& peerUid, int sessionType, quint64 expectedRecvSeq);

    // 清零未读数
    void slot_clearUnreadCount(const QString& peerUid, int sessionType);

    // 删除某个会话（不在列表显示了，但聊天记录还在）
    void slot_deleteConversation(const QString& peerUid, int sessionType);

    // 获取本地所有会话信息
    void slot_getAllConversations();

    /// 聊天记录表 (chat_history_local)
    // 追加一条聊天记录
    void slot_addChatHistory(const MsgInfo& msg);

    // 收到服务端 ACK 后，通过 client_msg_id 更新 msg_id 和 消息发送：成功状态
    void slot_updateMsgStatusAndVersion(std::shared_ptr<MsgInfo> msgInfo, std::shared_ptr<ConversationInfo> convInfo);

    void slot_updateMsgReadStatus(const QString &msgId, int isRead);

    void slot_updateAllMsgReadStatus(const QString &peerUid, int sessionType);

    void slot_insertGapMessages(const QString& peerUid, int sessionType, QList<MsgInfo> gapMsgs, bool hasMore);

    // 分页获取历史记录（极其重要：limit是每次拉取几条）
    // oldestTime 传 0 表示获取最新的一页
    void slot_getChatHistory(const QString& peerUid, int sessionType, qint64 oldestTime, int limit);

    // 自聊接口
    void slot_updateSelfChatReceiveInfo(const MsgInfo& msgInfo);

    // QMetaObject::invokeMethod接口
    /// 后续可以合并成一个getGlobalMaxVersionSync()接口，因为Version字段是全局递增的雪花ID
    Q_INVOKABLE quint64 getMaxFriendVersion();
    Q_INVOKABLE quint64 getMaxConversationVersion();
    Q_INVOKABLE quint64 getMaxFriendApplyVersion();

    Q_INVOKABLE QList<std::shared_ptr<UserInfo>> getAllFriendsSync();

    Q_INVOKABLE QList<ConversationInfo> getAllConversationsSync();

    Q_INVOKABLE void syncDeltaFriends(const QList<std::shared_ptr<UserInfo> > &deltaUserList);

    Q_INVOKABLE void syncDeltaConversations(const QList<ConversationInfo> &deltaConvList);

    Q_INVOKABLE bool appendMessageTransactionSync(const QList<MsgInfo>& msgList, const ConversationInfo& convInfo);

    Q_INVOKABLE QList<MsgInfo> getPendingMessagesSync();

    Q_INVOKABLE bool addOrUpdateUserInfoSync(std::shared_ptr<UserInfo> userInfo);

    Q_INVOKABLE bool addOrUpdateFriendSync(std::shared_ptr<UserInfo> userInfo);

    Q_INVOKABLE bool insertGapMessagesSync(const QString &peerUid, int sessionType, QList<MsgInfo> gapMsgs);

    Q_INVOKABLE quint64 getLocalMaxVersionSync(const QString &peerUid, int sessionType);

    Q_INVOKABLE bool upsertFriendApplySync(std::shared_ptr<FriendApplyInfo> applyInfo);

    Q_INVOKABLE bool updateAuthFriendSync(const QString& fromUid, const QString& toUid, int status, quint64 handleTime, quint64 version);

    Q_INVOKABLE bool updateFriendApplyVersionSync(const QString& fromUid, const QString& toUid, quint64 version);

    Q_INVOKABLE bool updateFriendListVersionSync(const QString &friendUid, quint64 version);

    Q_INVOKABLE QList<FriendApplyInfo> getAllFriendAppliesSync();

signals:
    void sig_init_success();

    void sig_init_failed();

    /// 用户信息缓存表 (user_info_local)
    void sig_addOrUpdateUserInfo_success();

    void sig_addOrUpdateUserInfo_failed();

    void sig_rspUserInfo(std::shared_ptr<UserInfo> userInfo);

    /// 好友关系表 (friend_list_local)
    void sig_addOrUpdateFriend_success();

    void sig_addOrUpdateFriend_failed();

    void sig_deleteFriend_success();

    void sig_deleteFriend_failed();

    void sig_getAllFriends(QList<std::shared_ptr<UserInfo>> friendList);

    /// 会话列表表 (conversation_local)
    void sig_appendMessageTransaction_success(const QList<MsgInfo>& msgList);

    void sig_appendMessageTransaction_failed(const QList<MsgInfo>& msgList);

    void sig_getSeq_success();

    void sig_getSeq_failed();

    void sig_getSeq_result(const QString& peerUid, int sessionType, uint64_t nextSendSeq, uint64_t expectedRecvSeq);

    void sig_clearUnreadCount_success();

    void sig_clearUnreadCount_failed();

    void sig_deleteConversation_success();

    void sig_deleteConversation_failed();

    void sig_getAllConversations(QList<ConversationInfo> convList);

    /// 聊天记录表 (chat_history_local)
    void sig_addChatHistory_success();

    void sig_addChatHistory_failed();

    void sig_updateMsgStatusAndVersion_success();

    void sig_updateMsgStatusAndVersion_failed();

    void sig_updateMsgReadStatus_success();

    void sig_updateMsgReadStatus_failed();

    void sig_updateAllMsgReadStatus_success();

    void sig_updateAllMsgReadStatus_failed();

    void sig_getChatHistory(QString peerUid, int sessionType, QList<MsgInfo> historyList);

    void sig_sync_history_received(QString peerUid, int sessionType, QList<MsgInfo> newMsgs, bool hasMore);

    /// 其他信号

private:
    QSqlDatabase getDb() const;

    bool createTables();

    bool doAddOrUpdateUserInfo(const QString& uid, const QString& username, const QString& email, const QString& avatarUrl, qint64 updateTime = QDateTime::currentMSecsSinceEpoch());

    bool doAddOrUpdateUserInfo(std::shared_ptr<UserInfo> userInfo);

    bool doAddChatHistory(const MsgInfo& msg);
    
    bool doAddChatHistoryList(const QList<MsgInfo>& msgList);

    const QString _connectionName = "yzc_chat_local_connection";

    QString _currentUid;
};

#endif // SQLWORKER_H
