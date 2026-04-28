#include "sqlitemgr.h"
SqliteMgr::SqliteMgr(QObject *parent)
    : QObject{parent}
{
    _thread = new QThread(this);
    _sqlWorker = new SqlWorker;
    _sqlWorker->moveToThread(_thread);
}

SqliteMgr::~SqliteMgr()
{
    if(_thread && _thread->isRunning()) {
        // 业务清理
        QMetaObject::invokeMethod(_sqlWorker, "slot_close", Qt::BlockingQueuedConnection);

        // 关闭线程
        _thread->quit();
        _thread->wait(1000);
    }
}

SqliteMgr &SqliteMgr::getInstance()
{
    static SqliteMgr instance;
    return instance;
}

void SqliteMgr::init()
{
    /// SqliteMgr ---> SqlWorker
    connect(_thread, &QThread::finished, _sqlWorker, &QObject::deleteLater);
    connect(this, &SqliteMgr::sig_initDb, _sqlWorker, &SqlWorker::slot_init);
    connect(this, &SqliteMgr::sig_closeDb, _sqlWorker, &SqlWorker::slot_close);
    connect(this, &SqliteMgr::sig_addOrUpdateUserInfo, _sqlWorker, &SqlWorker::slot_addOrUpdateUserInfo);
    connect(this, &SqliteMgr::sig_addOrUpdateFriend, _sqlWorker, &SqlWorker::slot_addOrUpdateFriend);
    connect(this, &SqliteMgr::sig_deleteFriend, _sqlWorker, &SqlWorker::slot_deleteFriend);
    connect(this, &SqliteMgr::sig_getAllFriends, _sqlWorker, &SqlWorker::slot_getAllFriends);
    connect(this, &SqliteMgr::sig_appendMessageTransaction, _sqlWorker, &SqlWorker::slot_appendMessageTransaction);
    connect(this, &SqliteMgr::sig_getSeq, _sqlWorker, &SqlWorker::slot_getSeq);
    connect(this, &SqliteMgr::sig_clearUnreadCount, _sqlWorker, &SqlWorker::slot_clearUnreadCount);
    connect(this, &SqliteMgr::sig_deleteConversation, _sqlWorker, &SqlWorker::slot_deleteConversation);
    connect(this, &SqliteMgr::sig_addChatHistory, _sqlWorker, &SqlWorker::slot_addChatHistory);
    connect(this, &SqliteMgr::sig_updateMsgStatusAndVersion, _sqlWorker, &SqlWorker::slot_updateMsgStatusAndVersion);
    connect(this, &SqliteMgr::sig_updateMsgReadStatus, _sqlWorker, &SqlWorker::slot_updateMsgReadStatus);
    connect(this, &SqliteMgr::sig_updateAllMsgReadStatus, _sqlWorker, &SqlWorker::slot_updateAllMsgReadStatus);
    connect(this, &SqliteMgr::sig_getChatHistory, _sqlWorker, &SqlWorker::slot_getChatHistory);
    connect(this, &SqliteMgr::sig_getAllConversations, _sqlWorker, &SqlWorker::slot_getAllConversations);
    connect(this, &SqliteMgr::sig_reqUserInfo, _sqlWorker, &SqlWorker::slot_reqUserInfo);
    connect(this, &SqliteMgr::sig_updateSelfChatReceiveInfo, _sqlWorker, &SqlWorker::slot_updateSelfChatReceiveInfo);
    connect(this, &SqliteMgr::sig_insertGapMessages, _sqlWorker, &SqlWorker::slot_insertGapMessages);
    connect(this, &SqliteMgr::sig_updateConvExpectedRecvSeq, _sqlWorker, &SqlWorker::slot_updateConvExpectedRecvSeq);

    /// SqlWorker ---> SqliteMgr
    connect(_sqlWorker, &SqlWorker::sig_getAllFriends, this, &SqliteMgr::sig_getAllFriendsResult);
    connect(_sqlWorker, &SqlWorker::sig_getChatHistory, this, &SqliteMgr::sig_getChatHistoryResult);
    connect(_sqlWorker, &SqlWorker::sig_getAllConversations, this, &SqliteMgr::sig_getAllConversationsResult);
    connect(_sqlWorker, &SqlWorker::sig_getSeq_result, this, &SqliteMgr::sig_getSeq_result);
    connect(_sqlWorker, &SqlWorker::sig_init_success, this, &SqliteMgr::sig_initDb_success);
    connect(_sqlWorker, &SqlWorker::sig_init_failed, this, &SqliteMgr::sig_initDb_failed);
    connect(_sqlWorker, &SqlWorker::sig_rspUserInfo, this, &SqliteMgr::sig_rspUserInfo);
    connect(_sqlWorker, &SqlWorker::sig_appendMessageTransaction_success, this, &SqliteMgr::sig_appendMessageTransaction_success);
    connect(_sqlWorker, &SqlWorker::sig_sync_history_received, this, &SqliteMgr::sig_sync_history_received);
    _thread->start();
}

void SqliteMgr::initDatabase(const QString &uid)
{
    emit sig_initDb(uid);
}

void SqliteMgr::closeDatabase()
{
    emit sig_closeDb();
}

/// 下面都是各个表的CRUD

/// 1.user_info_local表
void SqliteMgr::addOrUpdateUserInfo(const QString &uid, const QString &username, const QString &email, const QString &avatarUrl)
{
    emit sig_addOrUpdateUserInfo(uid, username, email, avatarUrl);
}

void SqliteMgr::reqUserInfo(const QString &uid)
{
    emit sig_reqUserInfo(uid);
}

/// 2.friend_list_local表
void SqliteMgr::addOrUpdateFriend(std::shared_ptr<UserInfo> userInfo)
{
    emit sig_addOrUpdateFriend(userInfo);
}

void SqliteMgr::deleteFriend(const QString &friendUid)
{
    emit sig_deleteFriend(friendUid);
}

void SqliteMgr::getAllFriends()
{
    emit sig_getAllFriends();
}

/// 3.conversation_local表
void SqliteMgr::appendMessageTransaction(const QList<MsgInfo>& msgList, const ConversationInfo& convInfo)
{
    emit sig_appendMessageTransaction(msgList, convInfo);
}

void SqliteMgr::getSeq(const QString &peerUid, int sessionType)
{
    emit sig_getSeq(peerUid, sessionType);
}

void SqliteMgr::updateConvExpectedRecvSeq(const QString &peerUid, int sessionType, quint64 expectRecvSeq)
{
    emit sig_updateConvExpectedRecvSeq(peerUid, sessionType, expectRecvSeq);
}

void SqliteMgr::clearUnreadCount(const QString &peerUid, int sessionType)
{
    emit sig_clearUnreadCount(peerUid, sessionType);
}

void SqliteMgr::deleteConversation(const QString &peerUid, int sessionType)
{
    emit sig_deleteConversation(peerUid, sessionType);
}

void SqliteMgr::getAllConversations()
{
    emit sig_getAllConversations();
}

/// 4.chat_history_local表
void SqliteMgr::addChatHistory(const MsgInfo &msg)
{
    emit sig_addChatHistory(msg);
}

void SqliteMgr::updateMsgStatusAndVersion(std::shared_ptr<MsgInfo> msgInfo, std::shared_ptr<ConversationInfo> convInfo)
{
    emit sig_updateMsgStatusAndVersion(msgInfo, convInfo);
}

void SqliteMgr::updateMsgReadStatus(const QString &msgId, int isRead)
{
    emit sig_updateMsgReadStatus(msgId, isRead);
}

void SqliteMgr::updateAllMsgReadStatus(const QString &peerUid, int sessionType)
{
    emit sig_updateAllMsgReadStatus(peerUid, sessionType);
}

void SqliteMgr::insertGapMessages(const QString &peerUid, int sessionType, QList<MsgInfo> gapMsgs, bool hasMore)
{
    emit sig_insertGapMessages(peerUid, sessionType, gapMsgs, hasMore);
}

void SqliteMgr::getChatHistory(const QString &peerUid, int sessionType, qint64 oldestTime, int limit)
{
    emit sig_getChatHistory(peerUid, sessionType, oldestTime, limit);
}

void SqliteMgr::updateSelfChatReceiveInfo(const MsgInfo &msgInfo)
{
    emit sig_updateSelfChatReceiveInfo(msgInfo);
}

quint64 SqliteMgr::getMaxFriendVersion()
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    quint64 result;
    QMetaObject::invokeMethod(_sqlWorker, "getMaxFriendVersion", Qt::BlockingQueuedConnection, Q_RETURN_ARG(quint64, result));
    return result;
}

quint64 SqliteMgr::getMaxConversationVersion()
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    quint64 result;
    QMetaObject::invokeMethod(_sqlWorker, "getMaxConversationVersion", Qt::BlockingQueuedConnection, Q_RETURN_ARG(quint64, result));
    return result;
}

quint64 SqliteMgr::getMaxFriendApplyVersion()
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    quint64 result = 0;
    QMetaObject::invokeMethod(_sqlWorker, &SqlWorker::getMaxFriendApplyVersion, Qt::BlockingQueuedConnection, Q_RETURN_ARG(quint64, result));
    return result;
}

QList<std::shared_ptr<UserInfo> > SqliteMgr::getAllFriendsSync()
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    QList<std::shared_ptr<UserInfo>> resultList;
    QMetaObject::invokeMethod(_sqlWorker, "getAllFriendsSync", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QList<std::shared_ptr<UserInfo>>, resultList));
    return resultList;
}

QList<ConversationInfo> SqliteMgr::getAllConversationsSync()
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    QList<ConversationInfo> resultList;
    QMetaObject::invokeMethod(_sqlWorker, "getAllConversationsSync", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QList<ConversationInfo>, resultList));
    return resultList;
}

QList<FriendApplyInfo> SqliteMgr::getAllFriendAppliesSync()
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    QList<FriendApplyInfo> resultList;
    QMetaObject::invokeMethod(_sqlWorker, &SqlWorker::getAllFriendAppliesSync,
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<FriendApplyInfo>, resultList));
    return resultList;
}

void SqliteMgr::syncDeltaFriends(const QList<std::shared_ptr<UserInfo> > &deltaUserList)
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    QMetaObject::invokeMethod(_sqlWorker, "syncDeltaFriends", Qt::BlockingQueuedConnection, Q_ARG(QList<std::shared_ptr<UserInfo> >, deltaUserList));
}

void SqliteMgr::syncDeltaConversations(const QList<ConversationInfo> &deltaConvList)
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    QMetaObject::invokeMethod(_sqlWorker, "syncDeltaConversations", Qt::BlockingQueuedConnection, Q_ARG(QList<ConversationInfo>, deltaConvList));
}

bool SqliteMgr::appendMessageTransactionSync(const QList<MsgInfo> &msgList, const ConversationInfo &convInfo)
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    bool result = false;
    QMetaObject::invokeMethod(_sqlWorker, "appendMessageTransactionSync",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              Q_ARG(QList<MsgInfo>, msgList),
                              Q_ARG(ConversationInfo, convInfo));
    return result;
}

bool SqliteMgr::addOrUpdateUserInfoSync(std::shared_ptr<UserInfo> userInfo)
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    bool result = false;
    QMetaObject::invokeMethod(_sqlWorker, &SqlWorker::addOrUpdateUserInfoSync,
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              userInfo);
    return result;
}

bool SqliteMgr::insertGapMessagesSync(const QString &peerUid, int sessionType, QList<MsgInfo> gapMsgs)
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    bool result = false;
    QMetaObject::invokeMethod(_sqlWorker, &SqlWorker::insertGapMessagesSync,
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              peerUid,
                              sessionType,
                              gapMsgs);
    return result;
}

QList<MsgInfo> SqliteMgr::getPendingMessagesSync()
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    QList<MsgInfo> resultList;
    QMetaObject::invokeMethod(_sqlWorker, "getPendingMessagesSync",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<MsgInfo>, resultList));
    return resultList;
}

bool SqliteMgr::addOrUpdateFriendSync(std::shared_ptr<UserInfo> userInfo)
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    bool result;
    QMetaObject::invokeMethod(_sqlWorker, &SqlWorker::addOrUpdateFriendSync,
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              userInfo);
    return result;
}

quint64 SqliteMgr::getLocalMaxVersionSync(const QString &peerUid, int sessionType)
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    quint64 result;
    QMetaObject::invokeMethod(_sqlWorker, &SqlWorker::getLocalMaxVersionSync,
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(quint64, result),
                              peerUid,
                              sessionType);
    return result;
}

bool SqliteMgr::upsertFriendApplySync(std::shared_ptr<FriendApplyInfo> applyInfo)
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    bool result;
    QMetaObject::invokeMethod(_sqlWorker, &SqlWorker::upsertFriendApplySync,
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              applyInfo);
    return result;
}

bool SqliteMgr::updateAuthFriendSync(const QString &fromUid, const QString& toUid, int status, quint64 handleTime, quint64 version)
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    bool result = false;
    QMetaObject::invokeMethod(_sqlWorker, &SqlWorker::updateAuthFriendSync,
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              fromUid,
                              toUid,
                              status,
                              handleTime,
                              version);

    return result;
}

bool SqliteMgr::updateFriendApplyVersionSync(const QString &fromUid, const QString& toUid, quint64 version)
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    bool result = false;
    QMetaObject::invokeMethod(_sqlWorker, &SqlWorker::updateFriendApplyVersionSync,
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              fromUid,
                              toUid,
                              version);
    return result;
}

bool SqliteMgr::updateFriendListVersionSync(const QString &friendUid, quint64 version)
{
    Q_ASSERT(QThread::currentThread() != _sqlWorker->thread());
    bool result = false;
    QMetaObject::invokeMethod(_sqlWorker, &SqlWorker::updateFriendListVersionSync,
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, result),
                              friendUid,
                              version);
    return result;
}

