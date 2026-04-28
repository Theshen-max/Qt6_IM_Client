#include "sqlworker.h"
#include "UserData_Layer/usermgr.h"

SqlWorker::SqlWorker(QObject *parent)
    : QObject{parent}
{
    connect(this, &SqlWorker::sig_init_failed, this, &SqlWorker::slot_close);
}

SqlWorker::~SqlWorker()
{
    slot_close();
}

void SqlWorker::slot_init(const QString &uid)
{
    if(uid.isEmpty()) {
        qWarning() << "[SqliteMgr] 传入的 UID 为空，无法初始化数据库！";
        emit sig_init_failed();
        return;
    }

    if(QSqlDatabase::contains(_connectionName)) {
        // 如果两次传入的是同一个 uid，并且数据库已经打开了，直接返回成功，不用重复初始化
        if(_currentUid == uid) {
            emit sig_init_success();
            return;
        }
        slot_close();
    }

    _currentUid = uid;

    // 获取标准路径，避免自定义位置造成不同客户端可能有不存在的目录或权限
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/ChatData";
    QDir dir;
    if(!dir.exists(dataPath)) {
        dir.mkpath(dataPath);
    }

    // 拼装出具体的 db 文件绝对路径：xxx/ChatData/msg_1018288.db
    QString dbPath = dataPath + QString("/msg_%1.db").arg(_currentUid);
    if (!QFile::exists(dbPath)) {
        qWarning() << "数据库不存在：" << dbPath;
    }
    qDebug() << "[SqliteMgr] (!!!!!!!!!!!!!!!!!!!!注意当前用户是否为新注册的用户，是则会生成新Sqlite)当前用户的本地数据库路径为:" << dbPath;

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", _connectionName);
    db.setDatabaseName(dbPath);

    if(!db.open()) {
        qWarning() << "[SqliteMgr] 数据库打开失败:" << db.lastError().text();
        emit sig_init_failed();
        return;
    }
    qDebug() << "[SqliteMgr] 数据库打开成功！";

    if(!createTables())
        emit sig_init_failed();
    else
        emit sig_init_success();
}

void SqlWorker::slot_close()
{
    if(QSqlDatabase::contains(_connectionName))
    {
        {
            QSqlDatabase db = QSqlDatabase::database(_connectionName);
            if(db.isOpen())
                db.close();
        }

        // removeDatabase时，不能有其他变量还指向底层连接，所以必须用局部{}来销毁db
        QSqlDatabase::removeDatabase(_connectionName);
        qDebug() << "[SqliteMgr] 数据库连接已安全清理。";
    }
    _currentUid.clear();
}

void SqlWorker::slot_addOrUpdateUserInfo(const QString &uid, const QString &username, const QString &email, const QString &avatarUrl)
{
    if(doAddOrUpdateUserInfo(uid, username, email, avatarUrl))
        emit sig_addOrUpdateUserInfo_success();
    else
        emit sig_addOrUpdateUserInfo_failed();
}

void SqlWorker::slot_reqUserInfo(const QString &uid)
{
    std::shared_ptr<UserInfo> userInfo{};
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_rspUserInfo(userInfo);
        return;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT * FROM user_info_local WHERE uid = :uid;
    )");

    query.bindValue(":uid", uid);
    if(query.exec() && query.next()) {
        userInfo = std::make_shared<UserInfo>();
        userInfo->_uid = query.value("uid").toString();
        userInfo->_username = query.value("username").toString();
        userInfo->_avatarUrl = query.value("avatar_url").toString();
        userInfo->_email = query.value("email").toString();
    }
    emit sig_rspUserInfo(userInfo);
}

void SqlWorker::slot_addOrUpdateFriend(std::shared_ptr<UserInfo> userInfo)
{

    if(!userInfo) {
        emit sig_addOrUpdateFriend_failed();
        return;
    }
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_addOrUpdateFriend_failed();
        return;
    }

    db.transaction();

    if(!doAddOrUpdateUserInfo(userInfo->_uid, userInfo->_username, userInfo->_email, userInfo->_avatarUrl, userInfo->_updateTime)){
        db.rollback();
        emit sig_addOrUpdateFriend_failed();
        return;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO friend_list_local (friend_uid, remark, status, create_time)
        VALUES (:friend_uid, :remark, :status, :create_time)
        ON CONFLICT(friend_uid) DO UPDATE SET
            remark = excluded.remark,
            status = excluded.status;
    )");

    query.bindValue(":friend_uid", userInfo->_uid);
    query.bindValue(":remark", userInfo->_remark);
    query.bindValue(":status", userInfo->_status);
    query.bindValue(":create_time", QDateTime::currentMSecsSinceEpoch());

    if(!query.exec()) {
        qWarning() << "[SqliteMgr] slot_addOrUpdateFriend 关系表失败:" << query.lastError().text();
        db.rollback();
        emit sig_addOrUpdateFriend_failed();
        return;
    }

    db.commit();
    emit sig_addOrUpdateFriend_success();
    return;
}

void SqlWorker::slot_deleteFriend(const QString &friendUid)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_deleteFriend_failed();
        return;
    }

    QSqlQuery query(db);
    // 逻辑删除：将状态置为 0，而不清理实体缓存
    query.prepare("UPDATE friend_list_local SET status = 0 WHERE friend_uid = :friend_uid");
    query.bindValue(":friend_uid", friendUid);

    if(!query.exec()) {
        emit sig_deleteFriend_failed();
        return;
    }
    emit sig_deleteFriend_success();
}

void SqlWorker::slot_getAllFriends()
{
    QList<std::shared_ptr<UserInfo>> friendList;
    QSqlDatabase db = getDb();
    if (!db.isOpen()) {
        emit sig_getAllFriends(friendList);
        return;
    }

    QSqlQuery query(db);
    QString sql = R"(
        SELECT
            f.friend_uid, u.username, u.email, u.avatar_url, f.remark
        FROM
            friend_list_local AS f
        INNER JOIN
            user_info_local AS u ON f.friend_uid = u.uid
        WHERE
            f.status = 1
    )";

    if(query.exec(sql))
    {
        while(query.next())
        {
            auto userInfo = std::make_shared<UserInfo>();
            userInfo->_uid = query.value("friend_uid").toString();
            userInfo->_username = query.value("username").toString();
            userInfo->_email = query.value("email").toString();
            userInfo->_remark = query.value("remark").toString();
            userInfo->_avatarUrl = query.value("avatar_url").toString();
            userInfo->_isFriend = true;
            userInfo->_status = 1;
            friendList.append(userInfo);
        }
    }
    else
    {
        qWarning() << "[SqliteMgr] 获取好友列表失败:" << query.lastError().text();
    }
    emit sig_getAllFriends(friendList);
}

void SqlWorker::slot_appendMessageTransaction(const QList<MsgInfo>& msgList, const ConversationInfo& convInfo)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_appendMessageTransaction_failed(msgList);
        return;
    }

    db.transaction();

    if(!doAddChatHistoryList(msgList)) {
        db.rollback();
        emit sig_appendMessageTransaction_failed(msgList);
        return;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO conversation_local (
            peer_uid, session_type, last_msg_sender_uid, last_msg_type,
            last_msg_content, unread_count, last_seq_id,
            next_send_seq, expected_recv_seq, update_time,
            is_pinned, is_muted, version
        ) VALUES (
            :peer_uid, :session_type, :sender_uid, :msg_type,
            :content, :unread_add, 0,
            :next_send, :expected_recv, :update_time,
            :is_pinned, :is_muted, :version
        )
        ON CONFLICT(peer_uid, session_type) DO UPDATE SET
            last_msg_sender_uid = excluded.last_msg_sender_uid,
            last_msg_type = excluded.last_msg_type,
            last_msg_content = excluded.last_msg_content,
            unread_count = conversation_local.unread_count + excluded.unread_count,
            last_seq_id = conversation_local.last_seq_id + 1,       -- 会话全局Seq自增
            next_send_seq = excluded.next_send_seq,                 -- 覆盖最新滑动窗口值
            expected_recv_seq = excluded.expected_recv_seq,         -- 覆盖最新滑动窗口值
            update_time = excluded.update_time,
            is_pinned = excluded.is_pinned,
            is_muted = excluded.is_muted,
            version = excluded.version;
    )");

    query.bindValue(":peer_uid", convInfo.peerUid);
    query.bindValue(":session_type", convInfo.sessionType);
    query.bindValue(":sender_uid", convInfo.lastMsgSenderUid);
    query.bindValue(":msg_type", convInfo.lastMsgType);
    query.bindValue(":content", convInfo.lastMsgContent);
    query.bindValue(":unread_add", convInfo.unreadCount);
    query.bindValue(":next_send", static_cast<uint64_t>(convInfo.nextSendSeq));
    query.bindValue(":expected_recv", static_cast<uint64_t>(convInfo.expectedRecvSeq));
    query.bindValue(":update_time", convInfo.updateTime); // 会话活跃时间直接对齐消息创建时间
    query.bindValue(":is_pinned", static_cast<int>(convInfo.isPinned));
    query.bindValue(":is_muted", static_cast<int>(convInfo.isMuted));
    query.bindValue(":version", static_cast<uint64_t>(convInfo.version));
    if(!query.exec()) {
        qWarning() << "[SqliteMgr] 事务回滚 - 更新会话状态失败:" << query.lastError().text();
        db.rollback();
        emit sig_appendMessageTransaction_failed(msgList);
        return;
    }

    // 提交事务
    if (!db.commit()) {
        qWarning() << "[SqliteMgr] 事务提交失败:" << db.lastError().text();
        emit sig_appendMessageTransaction_failed(msgList);
        return;
    }

    emit sig_appendMessageTransaction_success(msgList);
}

void SqlWorker::slot_getSeq(const QString &peerUid, int sessionType)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_getSeq_failed();
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT next_send_seq, expected_recv_seq FROM conversation_local WHERE peer_uid = :peer_uid AND session_type = :session_type");
    query.bindValue(":peer_uid", peerUid);
    query.bindValue(":session_type", sessionType);
    if(query.exec() && query.next()) {
        quint64 out_nextSendSeq = query.value("next_send_seq").toULongLong();
        quint64 out_expectedRecvSeq = query.value("expected_recv_seq").toULongLong();
        emit sig_getSeq_success();
        emit sig_getSeq_result(peerUid, sessionType, out_nextSendSeq, out_expectedRecvSeq);
        return;
    }
    emit sig_getSeq_failed();
}

void SqlWorker::slot_updateConvExpectedRecvSeq(const QString &peerUid, int sessionType, quint64 expectedRecvSeq)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) return;

    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE conversation_local
        SET expected_recv_seq = :seq
        WHERE peer_uid = :peer_uid AND session_type = :session_type
    )");

    query.bindValue(":seq", expectedRecvSeq);
    query.bindValue(":peer_uid", peerUid);
    query.bindValue(":session_type", sessionType);

    if(!query.exec()) {
        qWarning() << "[SqlWorker] 异步持久化 expected_recv_seq 失败:" << query.lastError().text();
    }
}

void SqlWorker::slot_clearUnreadCount(const QString &peerUid, int sessionType)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_clearUnreadCount_failed();
        return;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE conversation_local SET unread_count = 0 WHERE peer_uid = :peer_uid AND session_type = :session_type");
    query.bindValue(":peer_uid", peerUid);
    query.bindValue(":session_type", sessionType);

    if(!query.exec()){
        emit sig_clearUnreadCount_failed();
        return;
    }
    emit sig_clearUnreadCount_success();
}

void SqlWorker::slot_deleteConversation(const QString &peerUid, int sessionType)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_deleteConversation_failed();
        return;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM conversation_local WHERE peer_uid = :peer_uid AND session_type = :session_type");
    query.bindValue(":peer_uid", peerUid);
    query.bindValue(":session_type", sessionType);

    if(!query.exec()) {
        emit sig_deleteConversation_failed();
        return;
    }
    emit sig_deleteConversation_success();
}

void SqlWorker::slot_getAllConversations()
{
    QList<ConversationInfo> convList;
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_getAllConversations(convList);
        return;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT * FROM conversation_local
        ORDER BY is_pinned DESC, update_time DESC;
    )");

    if(query.exec()) {
        while(query.next()) {
            ConversationInfo info;
            info.peerUid = query.value("peer_uid").toString();
            info.sessionType = query.value("session_type").toInt();
            info.lastMsgSenderUid = query.value("last_msg_sender_uid").toString();
            info.lastMsgType = query.value("last_msg_type").toInt();
            info.lastMsgContent = query.value("last_msg_content").toString();
            info.unreadCount = query.value("unread_count").toInt();
            info.lastSeqId = query.value("last_seq_id").toULongLong();
            info.nextSendSeq = query.value("next_send_seq").toULongLong();
            info.expectedRecvSeq = query.value("expected_recv_seq").toULongLong();
            info.updateTime = query.value("update_time").toLongLong();
            info.isPinned = query.value("is_pinned").toBool();
            info.isMuted = query.value("is_muted").toBool();
            info.version = query.value("version").toULongLong();
            convList.append(info);
        }
    } else {
        qWarning() << "[SqlWorker] 获取会话列表失败:" << query.lastError().text();
    }
    emit sig_getAllConversations(convList);
}

void SqlWorker::slot_addChatHistory(const MsgInfo &msg)
{
    if(doAddChatHistory(msg))
        emit sig_addChatHistory_success();
    else
        emit sig_addChatHistory_failed();
}

void SqlWorker::slot_updateMsgStatusAndVersion(std::shared_ptr<MsgInfo> msgInfo, std::shared_ptr<ConversationInfo> convInfo)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_updateMsgStatusAndVersion_failed();
        return;
    }

    db.transaction();
    QSqlQuery query(db);

    query.prepare(R"(
        UPDATE chat_history_local
        SET msg_id = :msg_id, send_status = :status
        WHERE client_msg_id = :client_id
    )");
    query.bindValue(":msg_id", msgInfo->msgId);
    query.bindValue(":status", msgInfo->sendStatus);
    query.bindValue(":client_id", msgInfo->clientMsgId);

    if (!query.exec()) {
        qWarning() << "[SqliteMgr] 更新消息状态与版本失败:" << query.lastError().text();
        db.rollback();
        emit sig_updateMsgStatusAndVersion_failed();
        return;
    }

    query.prepare(R"(
        UPDATE conversation_local
        SET version = :version
        WHERE peer_uid = :peer_uid AND session_type = :session_type
    )");
    query.bindValue(":version", convInfo->version);
    query.bindValue(":peer_uid", convInfo->peerUid);
    query.bindValue(":session_type", convInfo->sessionType);

    if (!query.exec()) {
        qWarning() << "[SqliteMgr] 更新消息状态与版本失败:" << query.lastError().text();
        db.rollback();
        emit sig_updateMsgStatusAndVersion_failed();
        return;
    }

    db.commit();
    emit sig_updateMsgStatusAndVersion_success();
}

void SqlWorker::slot_updateMsgReadStatus(const QString &msgId, int isRead)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_updateMsgReadStatus_failed();
        return;
    }

    QSqlQuery query(db);

    // 通常已读回执都是带着全局 msgId 来的
    query.prepare("UPDATE chat_history_local SET is_read = :is_read WHERE msg_id = :msg_id");
    query.bindValue(":is_read", isRead);
    query.bindValue(":msg_id", msgId);

    if(!query.exec()) {
        emit sig_updateMsgReadStatus_failed();
        return;
    }
    emit sig_updateMsgReadStatus_success();
}

void SqlWorker::slot_updateAllMsgReadStatus(const QString &peerUid, int sessionType)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_updateAllMsgReadStatus_failed();
        return;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE chat_history_local SET is_read = 1 WHERE sender_uid = :peer_uid AND session_type = :session_type AND is_read = 0");
    query.bindValue(":peer_uid", peerUid);
    query.bindValue(":session_type", sessionType);

    if(!query.exec()) {
        emit sig_updateAllMsgReadStatus_failed();
        return;
    }
    emit sig_updateAllMsgReadStatus_success();
}

void SqlWorker::slot_insertGapMessages(const QString &peerUid, int sessionType, QList<MsgInfo> gapMsgs, bool hasMore)
{
    if (gapMsgs.isEmpty()) return;

    QSqlDatabase db = getDb();
    if (!db.isOpen()) return;

    db.transaction();
    QSqlQuery query(db);
    query.prepare(R"(
        INSERT OR IGNORE INTO chat_history_local
        (client_msg_id, msg_id, peer_uid, session_type, sender_uid, msg_type, msg_content, seq_id, send_status, create_time, is_read)
        VALUES (:client_msg_id, :msg_id, :peer_uid, :session_type, :sender_uid, :msg_type, :msg_content, :seq_id, :send_status, :create_time, :is_read)
    )");

    QVariantList clientMsgIdList, msgIdList, peerUidList, sessionTypeList, senderUidList, msgTypeList, msgContentList, seqIdList, sendStatusList, createTimeList, isReadList;

    size_t count = gapMsgs.size();
    clientMsgIdList.reserve(count);
    msgIdList.reserve(count);
    peerUidList.reserve(count);
    sessionTypeList.reserve(count);
    senderUidList.reserve(count);
    msgTypeList.reserve(count);
    msgContentList.reserve(count);
    seqIdList.reserve(count);
    sendStatusList.reserve(count);
    createTimeList.reserve(count);
    isReadList.reserve(count);

    for(const MsgInfo& msgInfo : gapMsgs) {
        clientMsgIdList << msgInfo.clientMsgId;
        msgIdList << msgInfo.msgId;
        peerUidList << msgInfo.peerUid;
        sessionTypeList << msgInfo.sessionType;
        senderUidList << msgInfo.senderUid;
        msgTypeList << msgInfo.msgType;
        msgContentList << msgInfo.msgData;
        seqIdList << msgInfo.seqId;
        sendStatusList << 1;
        createTimeList << msgInfo.createTime;
        isReadList << 1;
    }

    query.addBindValue(clientMsgIdList);
    query.addBindValue(msgIdList);
    query.addBindValue(peerUidList);
    query.addBindValue(sessionTypeList);
    query.addBindValue(senderUidList);
    query.addBindValue(msgTypeList);
    query.addBindValue(msgContentList);
    query.addBindValue(seqIdList);
    query.addBindValue(sendStatusList);
    query.addBindValue(createTimeList);
    query.addBindValue(isReadList);

    if(!query.exec()) {
        qWarning() << "[SqliteMgr] 事务回滚 - 更新会话状态失败:" << query.lastError().text();
        db.rollback();
        return;
    }

    if(db.commit()) {
        emit sig_sync_history_received(peerUid, sessionType, gapMsgs, hasMore);
    }
    else {
        qWarning() << "[SqlWorker] 插入增量历史消息失败:" << db.lastError().text();
        db.rollback();
    }
}

void SqlWorker::slot_getChatHistory(const QString &peerUid, int sessionType, qint64 oldestTime, int limit)
{
    QList<MsgInfo> list;
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        emit sig_getChatHistory(peerUid, sessionType, list);
        return;
    }

    QSqlQuery query(db);
    QString sql;
    QString selfUid = UserMgr::getInstance().uid(); // 获取自己的 UID

    if(oldestTime <= 0)
    {
        sql = R"(
            SELECT * FROM chat_history_local
            WHERE peer_uid = ? AND session_type = ?
            ORDER BY create_time DESC LIMIT ?
        )";
    }
    else
    {
        sql = R"(
            SELECT * FROM chat_history_local
            WHERE peer_uid = ? AND session_type = ? AND create_time < ?
            ORDER BY create_time DESC LIMIT ?
        )";
    }

    query.prepare(sql);

    query.addBindValue(peerUid);
    query.addBindValue(sessionType);
    if (oldestTime > 0)
        query.addBindValue(oldestTime);

    query.addBindValue(limit);

    // qDebug() << sql;
    // qDebug() << query.boundValues();
    // qDebug() << query.lastQuery();

    if(query.exec())
    {
        while(query.next())
        {
            MsgInfo msg;
            msg.clientMsgId = query.value("client_msg_id").toString();
            msg.msgId = query.value("msg_id").toString();
            msg.peerUid = query.value("peer_uid").toString();
            msg.sessionType = query.value("session_type").toInt();
            msg.senderUid = query.value("sender_uid").toString();
            msg.msgType = query.value("msg_type").toInt();
            msg.msgData = query.value("msg_content").toString();
            msg.seqId = query.value("seq_id").toULongLong();
            msg.sendStatus = query.value("send_status").toInt();
            msg.createTime = query.value("create_time").toLongLong();
            msg.isRead = query.value("is_read").toInt();
            msg.isSelf = msg.senderUid == selfUid;
            list.append(msg);
        }

        std::reverse(list.begin(), list.end());
    }
    else
        qWarning() << "[SqliteMgr] slot_获取聊天记录失败:" << query.lastError().text();

    emit sig_getChatHistory(peerUid, sessionType, list);
}

void SqlWorker::slot_updateSelfChatReceiveInfo(const MsgInfo &msgInfo)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) return;

    db.transaction();
    QSqlQuery query(db);

    query.prepare(R"(
        UPDATE chat_history_local
        SET msg_id = :msg_id,
            is_read = 1
        WHERE client_msg_id = :client_msg_id
    )");
    query.bindValue(":msg_id", msgInfo.msgId);
    query.bindValue(":client_msg_id", msgInfo.clientMsgId);

    if (!query.exec()) {
        qWarning() << "[SqlWorker] 自聊消息历史状态更新失败:" << query.lastError().text();
        db.rollback();
        return;
    }

    query.prepare(R"(
        UPDATE conversation_local
        SET version = MAX(version, :version)
        WHERE peer_uid = :peer_uid AND session_type = :session_type
    )");
    query.bindValue(":version", msgInfo.msgId.toULongLong());
    query.bindValue(":peer_uid", msgInfo.peerUid);
    query.bindValue(":session_type", msgInfo.sessionType);

    if (!query.exec()) {
        qWarning() << "[SqlWorker] 自聊会话状态更新失败:" << query.lastError().text();
        db.rollback();
        return;
    }

    db.commit();
}

QSqlDatabase SqlWorker::getDb() const
{
    return QSqlDatabase::database(_connectionName);
}

bool SqlWorker::createTables()
{
    QSqlDatabase db = getDb();

    if(!db.isOpen()) return false;

    QSqlQuery query(db);

    // 1. 用户信息缓存表
    QString sqlUserInfo = R"(
        CREATE TABLE IF NOT EXISTS user_info_local (
            uid TEXT PRIMARY KEY,
            username TEXT NOT NULL,
            avatar_url TEXT,
            email TEXT,
            update_time INTEGER NOT NULL,
            version INTEGER DEFAULT 0 NOT NULL
        );
    )";
    if (!query.exec(sqlUserInfo)) {
        qWarning() << "创建 user_info_local 失败:" << query.lastError().text();
        return false;
    }

    // 建立 version 降序索引
    if(!query.exec(R"(CREATE INDEX IF NOT EXISTS idx_user_version ON user_info_local(version DESC);)")) {
        qWarning() << "创建 user_info_local 版本索引失败:" << query.lastError().text();
        return false;
    }

    // 2. 好友关系表
    QString sqlFriendList = R"(
        CREATE TABLE IF NOT EXISTS friend_list_local (
            friend_uid TEXT PRIMARY KEY,
            remark TEXT DEFAULT '',
            status INTEGER DEFAULT 1,
            create_time INTEGER DEFAULT 0 NOT NULL,
            version INTEGER DEFAULT 0 NOT NULL
        );
    )";

    if (!query.exec(sqlFriendList)) {
        qWarning() << "创建 friend_list_local 失败:" << query.lastError().text();
        return false;
    }

    if(!query.exec(R"(CREATE INDEX IF NOT EXISTS idx_friend_status ON friend_list_local(status);)")) {
        qWarning() << "创建 friend_list_local的索引失败:" << query.lastError().text();
        return false;
    }

    // 建立 version 降序索引
    if(!query.exec(R"(CREATE INDEX IF NOT EXISTS idx_friend_version ON friend_list_local(version DESC);)")) {
        qWarning() << "创建 friend_list_local 版本索引失败:" << query.lastError().text();
        return false;
    }

    // 3. 会话表
    QString sqlConversation = R"(
        CREATE TABLE IF NOT EXISTS conversation_local (
            peer_uid TEXT NOT NULL,            -- 聊天对象ID（好友UID或群组ID）
            session_type INTEGER NOT NULL,        -- 会话类型：1-单聊，2-群聊
            last_msg_sender_uid TEXT NOT NULL, -- 最后一条消息的真实发送者ID
            last_msg_type INTEGER DEFAULT 1,      -- 消息类型：1-文本，2-图片，3-文件等
            last_msg_content TEXT,                -- 最后一条消息的内容摘要
            unread_count INTEGER DEFAULT 0,       -- 未读消息数
            last_seq_id INTEGER DEFAULT 1,        -- 当前会话本地最新的消息序列号（同步核心）
            next_send_seq INTEGER DEFAULT 1,     -- TCP 滑动窗口: 发送序列号
            expected_recv_seq INTEGER DEFAULT 1, -- TCP 滑动窗口: 期待接收序列号
            update_time INTEGER NOT NULL,         -- 会话最后活跃时间戳（毫秒级）
            is_pinned INTEGER DEFAULT 0,          -- 是否置顶：0-否，1-是
            is_muted INTEGER DEFAULT 0,           -- 是否免打扰：0-否，1-是
            version INTEGER DEFAULT 0 NOT NULL,
            PRIMARY KEY (peer_uid, session_type)
        );
    )";

    if (!query.exec(sqlConversation)) {
        qWarning() << "创建 conversation_local 失败:" << query.lastError().text();
        return false;
    }

    if(!query.exec(R"(CREATE INDEX IF NOT EXISTS idx_conv_list_sort ON conversation_local(is_pinned DESC, update_time DESC);)")) {
        qWarning() << "创建 conversation_local 的索引失败:" << query.lastError().text();
        return false;
    }

    // 建立 version 降序索引
    if(!query.exec(R"(CREATE INDEX IF NOT EXISTS idx_conv_version ON conversation_local(version DESC);)")) {
        qWarning() << "创建 conversation_local 版本索引失败:" << query.lastError().text();
        return false;
    }

    // 4.创建聊天历史表
    QString sqlChatHistory = R"(
        CREATE TABLE IF NOT EXISTS chat_history_local (
            client_msg_id TEXT PRIMARY KEY,      -- 客户端生成的 UUID
            msg_id TEXT UNIQUE,                  -- 服务端生成的 雪花ID (可为空)
            peer_uid TEXT NOT NULL,
            session_type INTEGER NOT NULL,
            sender_uid TEXT NOT NULL,
            msg_type INTEGER DEFAULT 1,
            msg_content TEXT,
            seq_id INTEGER,
            send_status INTEGER DEFAULT 1,       -- 0发送中, 1成功, 2失败
            is_read INTEGER DEFAULT 0,
            create_time INTEGER NOT NULL
        );
    )";

    if(!query.exec(sqlChatHistory)) {
        qWarning() << "创建 chat_history_local 失败:" << query.lastError().text();
        return false;
    }

    if(!query.exec(R"(CREATE INDEX IF NOT EXISTS idx_chat_pull ON chat_history_local(peer_uid, session_type, create_time DESC);)")) {
        qWarning() << "创建 chat_history_local 的索引失败:" << query.lastError().text();
        return false;
    }

    if(!query.exec(R"(CREATE INDEX IF NOT EXISTS idx_is_read ON chat_history_local(peer_uid, session_type, is_read);)")) {
        qWarning() << "创建 chat_history_local 的索引失败:" << query.lastError().text();
        return false;
    }

    if(!query.exec(R"(CREATE INDEX IF NOT EXISTS idx_sender_uid_send_status_create_time ON chat_history_local(sender_uid, send_status, create_time);)")) {
        qWarning() << "创建 chat_history_local 的索引失败:" << query.lastError().text();
        return false;
    }

    if(!query.exec(R"(CREATE INDEX IF NOT EXISTS idx_sender_uid_peer_uid_session_type_create_time ON chat_history_local(sender_uid, peer_uid, session_type, create_time);)")) {
        qWarning() << "创建 chat_history_local 的索引失败:" << query.lastError().text();
        return false;
    }


    // 5.创建好友申请表
    QString sqlFriendApply = R"(
        CREATE TABLE IF NOT EXISTS friend_apply_local (
            from_uid TEXT,
            to_uid TEXT,
            status INTEGER,
            greeting TEXT,
            apply_time INTEGER,
            handle_time INTEGER,
            version INTEGER,
            PRIMARY KEY(from_uid, to_uid)
        )
    )";

    if(!query.exec(sqlFriendApply)){
        qWarning() << "创建 friend_apply_local 表失败:" << query.lastError().text();
        return false;
    }


    if(!query.exec(R"(CREATE INDEX IF NOT EXISTS idx_apply_version ON friend_apply_local(version);)")) {
        qWarning() << "创建 friend_apply_local 索引失败:" << query.lastError().text();
        return false;
    }

    qDebug() << "[SqliteMgr] 所有数据表检查/创建完毕！";
    return true;
}

bool SqlWorker::doAddOrUpdateUserInfo(const QString &uid, const QString &username, const QString &email, const QString &avatarUrl, qint64 updateTime)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO user_info_local (uid, username, email, avatar_url, update_time)
        VALUES (:uid, :username, :email, :avatar_url, :update_time)
        ON CONFLICT(uid) DO UPDATE SET
            username = excluded.username,
            email = excluded.email,
            avatar_url = excluded.avatar_url,
            update_time = excluded.update_time;
        WHERE excluded.update_time > user_info_local.update_time
    )");
    query.bindValue(":uid", uid);
    query.bindValue(":username", username);
    query.bindValue(":email", email);
    query.bindValue(":avatar_url", avatarUrl);
    query.bindValue(":update_time", updateTime);

    if(!query.exec()) {
        qWarning() << "[SqliteMgr] addOrUpdateUserInfo 失败:" << query.lastError().text();
        return false;
    }

    return true;
}

bool SqlWorker::doAddOrUpdateUserInfo(std::shared_ptr<UserInfo> userInfo)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO user_info_local (uid, username, email, avatar_url, update_time, version)
        VALUES (:uid, :username, :email, :avatar_url, :update_time, :version)
        ON CONFLICT(uid) DO UPDATE SET
            username = excluded.username,
            email = excluded.email,
            avatar_url = excluded.avatar_url,
            update_time = excluded.update_time,
            version = excluded.version
        WHERE excluded.version > user_info_local.version
    )");
    query.bindValue(":uid", userInfo->_uid);
    query.bindValue(":username", userInfo->_username);
    query.bindValue(":email", userInfo->_email);
    query.bindValue(":avatar_url", userInfo->_avatarUrl);
    query.bindValue(":update_time", userInfo->_updateTime);
    query.bindValue(":version", userInfo->_version);

    if(!query.exec()) {
        qWarning() << "[SqliteMgr] addOrUpdateUserInfo 失败:" << query.lastError().text();
        return false;
    }

    return true;
}

bool SqlWorker::doAddChatHistory(const MsgInfo &msg)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT OR IGNORE INTO chat_history_local (
            client_msg_id, msg_id, peer_uid, session_type, sender_uid,
            msg_type, msg_content, seq_id, send_status, create_time, is_read
        ) VALUES (
            :client_msg_id, :msg_id, :peer_uid, :session_type, :sender_uid,
            :msg_type, :msg_content, :seq_id, :send_status, :create_time, :is_read
        );
    )");

    query.bindValue(":client_msg_id", msg.clientMsgId);
    query.bindValue(":msg_id", msg.msgId);
    query.bindValue(":peer_uid", msg.peerUid);
    query.bindValue(":session_type", msg.sessionType);
    query.bindValue(":sender_uid", msg.senderUid);
    query.bindValue(":msg_type", msg.msgType);
    query.bindValue(":msg_content", msg.msgData);
    query.bindValue(":seq_id", static_cast<quint64>(msg.seqId));
    query.bindValue(":send_status", msg.sendStatus);
    query.bindValue(":create_time", msg.createTime);
    query.bindValue(":is_read", msg.isSelf ? 1 : 0);

    if(!query.exec()) {
        qWarning() << "[SqliteMgr] 插入聊天记录失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool SqlWorker::doAddChatHistoryList(const QList<MsgInfo> &msgList)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen() || msgList.isEmpty()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT OR IGNORE INTO chat_history_local (
            client_msg_id, msg_id, peer_uid, session_type, sender_uid,
            msg_type, msg_content, seq_id, send_status, create_time, is_read
        ) VALUES (
            ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?
        );
    )");

    // 准备批量绑定的数据列
    QVariantList clientMsgIdList;
    QVariantList msgIdList;
    QVariantList peerUidList;
    QVariantList sessionTypeList;
    QVariantList senderUidList;
    QVariantList msgTypeList;
    QVariantList msgContentList;
    QVariantList seqIdList;
    QVariantList sendStatusList;
    QVariantList createTimeList;
    QVariantList isReadList;

    // 预分配内存，提升大批量插入时的速度
    int count = msgList.size();
    clientMsgIdList.reserve(count);
    msgIdList.reserve(count);
    peerUidList.reserve(count);
    sessionTypeList.reserve(count);
    senderUidList.reserve(count);
    msgTypeList.reserve(count);
    msgContentList.reserve(count);
    seqIdList.reserve(count);
    sendStatusList.reserve(count);
    createTimeList.reserve(count);
    isReadList.reserve(count);

    // 将结构体列表拆解为列数组
    for(const MsgInfo& msg : msgList) {
        clientMsgIdList << msg.clientMsgId;
        msgIdList << msg.msgId;
        peerUidList << msg.peerUid;
        sessionTypeList << msg.sessionType;
        senderUidList << msg.senderUid;
        msgTypeList << msg.msgType;
        msgContentList << msg.msgData;
        seqIdList << static_cast<quint64>(msg.seqId);
        sendStatusList << msg.sendStatus;
        createTimeList << msg.createTime;
        isReadList << (msg.isSelf ? 1 : 0);
    }

    // 严格按照 VALUES 中的 ? 顺序添加绑定
    query.addBindValue(clientMsgIdList);
    query.addBindValue(msgIdList);
    query.addBindValue(peerUidList);
    query.addBindValue(sessionTypeList);
    query.addBindValue(senderUidList);
    query.addBindValue(msgTypeList);
    query.addBindValue(msgContentList);
    query.addBindValue(seqIdList);
    query.addBindValue(sendStatusList);
    query.addBindValue(createTimeList);
    query.addBindValue(isReadList);

    // 执行批量插入
    if(!query.execBatch()) {
        qWarning() << "[SqliteMgr] 批量插入聊天记录失败:" << query.lastError().text();
        return false;
    }

    return true;
}

quint64 SqlWorker::getMaxFriendVersion()
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) return 0;

    QSqlQuery query(db);
    if(query.exec(R"(SELECT MAX(version) as version FROM friend_list_local)") && query.next()) {
        return query.value("version").toULongLong();
    }
    return 0;
}

quint64 SqlWorker::getMaxConversationVersion()
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) return 0;

    QSqlQuery query(db);
    if(query.exec(R"(SELECT MAX(version) as version FROM conversation_local)") && query.next()) {
        return query.value("version").toULongLong();
    }
    return 0;
}

quint64 SqlWorker::getMaxFriendApplyVersion()
{
    quint64 maxVer = 0;
    QSqlDatabase db = getDb();
    if(!db.isOpen()) return 0;

    QSqlQuery query(db);
    query.prepare("SELECT MAX(version) FROM friend_apply_local");
    if(query.exec() && query.next()) {
        maxVer = query.value(0).toULongLong();
    }
    return maxVer;
}

QList<std::shared_ptr<UserInfo>> SqlWorker::getAllFriendsSync()
{
    QList<std::shared_ptr<UserInfo>> friendList;
    QSqlDatabase db = getDb();
    if (!db.isOpen()) {
        return friendList;
    }

    QSqlQuery query(db);
    QString sql = R"(
        SELECT
            f.friend_uid, u.username, u.email, u.avatar_url, f.remark
        FROM
            friend_list_local AS f
        INNER JOIN
            user_info_local AS u ON f.friend_uid = u.uid
        WHERE
            f.status = 1
    )";

    if(query.exec(sql))
    {
        while(query.next())
        {
            auto userInfo = std::make_shared<UserInfo>();
            userInfo->_uid = query.value("friend_uid").toString();
            userInfo->_username = query.value("username").toString();
            userInfo->_email = query.value("email").toString();
            userInfo->_remark = query.value("remark").toString();
            userInfo->_avatarUrl = query.value("avatar_url").toString();
            userInfo->_isFriend = true;
            userInfo->_status = 1;
            friendList.append(userInfo);
        }
    }
    else
    {
        qWarning() << "[SqliteMgr] 获取好友列表失败:" << query.lastError().text();
    }
    return friendList;
}

QList<ConversationInfo> SqlWorker::getAllConversationsSync()
{
    QList<ConversationInfo> convList;
    QSqlDatabase db = getDb();
    if(!db.isOpen()) {
        return convList;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT * FROM conversation_local
        ORDER BY is_pinned DESC, update_time DESC;
    )");

    if(query.exec()) {
        while(query.next()) {
            ConversationInfo info;
            info.peerUid = query.value("peer_uid").toString();
            info.sessionType = query.value("session_type").toInt();
            info.lastMsgSenderUid = query.value("last_msg_sender_uid").toString();
            info.lastMsgType = query.value("last_msg_type").toInt();
            info.lastMsgContent = query.value("last_msg_content").toString();
            info.unreadCount = query.value("unread_count").toInt();
            info.lastSeqId = query.value("last_seq_id").toULongLong();
            info.nextSendSeq = query.value("next_send_seq").toULongLong();
            info.expectedRecvSeq = query.value("expected_recv_seq").toULongLong();
            info.updateTime = query.value("update_time").toLongLong();
            info.isPinned = query.value("is_pinned").toBool();
            info.isMuted = query.value("is_muted").toBool();
            info.version = query.value("version").toULongLong();
            convList.append(info);
        }
    }
    else {
        qWarning() << "[SqlWorker] 获取会话列表失败:" << query.lastError().text();
    }
    return convList;
}

void SqlWorker::syncDeltaFriends(const QList<std::shared_ptr<UserInfo> > &deltaUserList)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen() || deltaUserList.isEmpty()) return;

    db.transaction();

    QSqlQuery qUser(db);
    qUser.prepare(R"(
        INSERT INTO user_info_local (uid, username, email, avatar_url, update_time, version)
        VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(uid) DO UPDATE SET
            username = excluded.username,
            email = excluded.email,
            avatar_url = excluded.avatar_url,
            update_time = excluded.update_time,
            version = excluded.version
        WHERE excluded.version > user_info_local.version;
    )");

    QVariantList u_uid, u_name, u_email, u_avatar, u_updateTime, u_version;

    QSqlQuery qFriend(db);
    qFriend.prepare(R"(
        INSERT INTO friend_list_local (friend_uid, remark, status, create_time, version)
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(friend_uid) DO UPDATE SET
            remark = excluded.remark,
            status = excluded.status,
            version = excluded.version
        WHERE excluded.version > friend_list_local.version;
    )");

    QVariantList f_uid, f_remark, f_status, f_createTime, f_version;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (const auto& user : deltaUserList) {
        if (!user) continue;

        // user_info 绑定
        u_uid << user->_uid;
        u_name << user->_username;
        u_email << user->_email;
        u_avatar << user->_avatarUrl;
        u_updateTime << now;
        u_version << user->_version;

        // friend_list 绑定
        f_uid << user->_uid;
        f_remark << user->_remark;
        f_status << user->_status; // 服务器传 0 就是逻辑删除
        f_createTime << user->_createTime;
        f_version << user->_version;
    }

    qUser.addBindValue(u_uid);
    qUser.addBindValue(u_name);
    qUser.addBindValue(u_email);
    qUser.addBindValue(u_avatar);
    qUser.addBindValue(u_updateTime);
    qUser.addBindValue(u_version);

    if (!qUser.execBatch()) {
        qWarning() << "[SqlWorker] 增量更新 user_info 失败:" << qUser.lastError().text();
        db.rollback();
        return;
    }

    qFriend.addBindValue(f_uid);
    qFriend.addBindValue(f_remark);
    qFriend.addBindValue(f_status);
    qFriend.addBindValue(f_createTime);
    qFriend.addBindValue(f_version);
    if (!qFriend.execBatch()) {
        qWarning() << "[SqlWorker] 增量更新 friend_list 失败:" << qFriend.lastError().text();
        db.rollback();
        return;
    }
    db.commit();
}

void SqlWorker::syncDeltaConversations(const QList<ConversationInfo> &deltaConvList)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen() || deltaConvList.isEmpty()) return;

    db.transaction();

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO conversation_local (
            peer_uid, session_type, last_msg_sender_uid, last_msg_type,
            last_msg_content, unread_count, last_seq_id, update_time, version
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(peer_uid, session_type) DO UPDATE SET
            last_msg_sender_uid = excluded.last_msg_sender_uid,
            last_msg_type = excluded.last_msg_type,
            last_msg_content = excluded.last_msg_content,
            unread_count = excluded.unread_count,
            last_seq_id = excluded.last_seq_id,
            update_time = excluded.update_time,
            version = excluded.version
        WHERE excluded.version > conversation_local.version;
    )");

    QVariantList peerUid, sessionType, senderUid, msgType, msgContent, unread, seqId, updateTime, version;

    for (const auto& conv : deltaConvList) {
        peerUid << conv.peerUid;
        sessionType << conv.sessionType;
        senderUid << conv.lastMsgSenderUid;
        msgType << conv.lastMsgType;
        msgContent << conv.lastMsgContent;
        unread << conv.unreadCount;
        seqId << conv.lastSeqId;
        updateTime << conv.updateTime;
        version << conv.version;
    }

    query.addBindValue(peerUid);
    query.addBindValue(sessionType);
    query.addBindValue(senderUid);
    query.addBindValue(msgType);
    query.addBindValue(msgContent);
    query.addBindValue(unread);
    query.addBindValue(seqId);
    query.addBindValue(updateTime);
    query.addBindValue(version);

    if (!query.execBatch()) {
        qWarning() << "[SqlWorker] 增量更新会话列表失败:" << query.lastError().text();
        db.rollback();
        return;
    }

    db.commit();
}

bool SqlWorker::appendMessageTransactionSync(const QList<MsgInfo> &msgList, const ConversationInfo &convInfo)
{
    QSqlDatabase db = getDb();
    if(!db.isOpen()) return false;

    db.transaction();

    if(!doAddChatHistoryList(msgList)) {
        db.rollback();
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO conversation_local (
            peer_uid, session_type, last_msg_sender_uid, last_msg_type,
            last_msg_content, unread_count, last_seq_id,
            next_send_seq, expected_recv_seq, update_time,
            is_pinned, is_muted, version
        ) VALUES (
            :peer_uid, :session_type, :sender_uid, :msg_type,
            :content, :unread_add, 0,
            :next_send, :expected_recv, :update_time,
            :is_pinned, :is_muted, :version
        )
        ON CONFLICT(peer_uid, session_type) DO UPDATE SET
            last_msg_sender_uid = excluded.last_msg_sender_uid,
            last_msg_type = excluded.last_msg_type,
            last_msg_content = excluded.last_msg_content,
            unread_count = conversation_local.unread_count + excluded.unread_count,
            last_seq_id = conversation_local.last_seq_id + 1,       -- 会话全局Seq自增
            next_send_seq = excluded.next_send_seq,                 -- 覆盖最新滑动窗口值
            expected_recv_seq = excluded.expected_recv_seq,         -- 覆盖最新滑动窗口值
            update_time = excluded.update_time,
            is_pinned = excluded.is_pinned,
            is_muted = excluded.is_muted,
            version = excluded.version;
    )");

    query.bindValue(":peer_uid", convInfo.peerUid);
    query.bindValue(":session_type", convInfo.sessionType);
    query.bindValue(":sender_uid", convInfo.lastMsgSenderUid);
    query.bindValue(":msg_type", convInfo.lastMsgType);
    query.bindValue(":content", convInfo.lastMsgContent);
    query.bindValue(":unread_add", convInfo.unreadCount);
    query.bindValue(":next_send", static_cast<qulonglong>(convInfo.nextSendSeq));
    query.bindValue(":expected_recv", static_cast<qulonglong>(convInfo.expectedRecvSeq));
    query.bindValue(":update_time", convInfo.updateTime); // 会话活跃时间直接对齐消息创建时间
    query.bindValue(":is_pinned", static_cast<int>(convInfo.isPinned));
    query.bindValue(":is_muted", static_cast<int>(convInfo.isMuted));
    query.bindValue(":version", convInfo.version);

    if(!query.exec()) {
        qWarning() << "[SqlWorker] 事务回滚 - 更新会话状态失败:" << query.lastError().text();
        db.rollback();
        return false;
    }

    return db.commit(); // 提交成功返回 true
}

QList<MsgInfo> SqlWorker::getPendingMessagesSync()
{
    QList<MsgInfo> list;
    QSqlDatabase db = getDb();
    if(!db.isOpen()) return list;

    QSqlQuery query(db);
    query.prepare("SELECT * FROM chat_history_local WHERE sender_uid = ? AND send_status = 0 ORDER BY create_time ASC");
    query.bindValue(0, UserMgr::getInstance().uid());
    if(query.exec()) {
        while(query.next()) {
            MsgInfo msg;
            msg.clientMsgId = query.value("client_msg_id").toString();
            msg.msgId = query.value("msg_id").toString();
            msg.peerUid = query.value("peer_uid").toString();
            msg.sessionType = query.value("session_type").toInt();
            msg.senderUid = query.value("sender_uid").toString();
            msg.msgType = query.value("msg_type").toInt();
            msg.msgData = query.value("msg_content").toString();
            msg.seqId = query.value("seq_id").toULongLong();
            msg.sendStatus = query.value("send_status").toInt();
            msg.createTime = query.value("create_time").toULongLong();
            msg.isRead = query.value("is_read").toInt();
            msg.isSelf = true;
            list.append(msg);
        }
    }
    return list;
}

bool SqlWorker::addOrUpdateUserInfoSync(std::shared_ptr<UserInfo> userInfo)
{
    return doAddOrUpdateUserInfo(userInfo);
}

bool SqlWorker::addOrUpdateFriendSync(std::shared_ptr<UserInfo> userInfo)
{
    if(!userInfo)
        return false;

    QSqlDatabase db = getDb();
    if(!db.isOpen())
        return false;

    db.transaction();

    if(!doAddOrUpdateUserInfo(userInfo)){
        db.rollback();
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO friend_list_local (friend_uid, status, create_time, version)
        VALUES (:friend_uid, :status, :create_time, :version)
        ON CONFLICT(friend_uid) DO UPDATE SET
            remark = excluded.remark,
            status = excluded.status,
            version = excluded.version
        WHERE excluded.version > version;
    )");

    query.bindValue(":friend_uid", userInfo->_uid);
    query.bindValue(":status", userInfo->_status);
    query.bindValue(":create_time", userInfo->_createTime);
    query.bindValue(":version", userInfo->_version);

    if(!query.exec()) {
        qWarning() << "[SqliteMgr] addOrUpdateFriendSync 关系表失败:" << query.lastError().text();
        db.rollback();
        return false;
    }

    db.commit();
    return true;
}

bool SqlWorker::insertGapMessagesSync(const QString &peerUid, int sessionType, QList<MsgInfo> gapMsgs)
{
    if(peerUid.isEmpty()) return false;

    QSqlDatabase db = getDb();
    if(!db.isOpen())
        return false;

    db.transaction();
    QSqlQuery query(db);
    query.prepare(R"(
        INSERT OR IGNORE INTO chat_history_local
        (client_msg_id, msg_id, peer_uid, session_type, sender_uid, msg_type, msg_content, seq_id, send_status, create_time, is_read)
        VALUES (:client_msg_id, :msg_id, :peer_uid, :session_type, :sender_uid, :msg_type, :msg_content, :seq_id, :send_status, :create_time, :is_read)
    )");

    QVariantList clientMsgIdList, msgIdList, peerUidList, sessionTypeList, senderUidList, msgTypeList, msgContentList, seqIdList, sendStatusList, createTimeList, isReadList;

    size_t count = gapMsgs.size();
    clientMsgIdList.reserve(count);
    msgIdList.reserve(count);
    peerUidList.reserve(count);
    sessionTypeList.reserve(count);
    senderUidList.reserve(count);
    msgTypeList.reserve(count);
    msgContentList.reserve(count);
    seqIdList.reserve(count);
    sendStatusList.reserve(count);
    createTimeList.reserve(count);
    isReadList.reserve(count);

    for(const MsgInfo& msgInfo : gapMsgs) {
        clientMsgIdList << msgInfo.clientMsgId;
        msgIdList << msgInfo.msgId;
        peerUidList << msgInfo.peerUid;
        sessionTypeList << msgInfo.sessionType;
        senderUidList << msgInfo.senderUid;
        msgTypeList << msgInfo.msgType;
        msgContentList << msgInfo.msgData;
        seqIdList << msgInfo.seqId;
        sendStatusList << 1;
        createTimeList << msgInfo.createTime;
        isReadList << 1;
    }

    query.bindValue(":client_msg_id", clientMsgIdList);
    query.bindValue(":msg_id", msgIdList);
    query.bindValue(":peer_uid", peerUidList);
    query.bindValue(":session_type", sessionTypeList);
    query.bindValue(":sender_uid", senderUidList);
    query.bindValue(":msg_type", msgTypeList);
    query.bindValue(":msg_content", msgContentList);
    query.bindValue(":seq_id", seqIdList);
    query.bindValue(":send_status", sendStatusList);
    query.bindValue(":create_time", createTimeList);
    query.bindValue(":is_read", isReadList);

    if(!query.execBatch()) {
        qWarning() << "[SqliteMgr] 事务回滚 - 更新会话状态失败:" << query.lastError().text();
        db.rollback();
        return false;
    }

    if(db.commit()) {
        return true;
    }
    else {
        qWarning() << "[SqlWorker] 插入增量历史消息失败:" << db.lastError().text();
        db.rollback();
        return false;
    }
}

quint64 SqlWorker::getLocalMaxVersionSync(const QString &peerUid, int sessionType)
{
    if(peerUid.isEmpty()) return 0;

    QSqlDatabase db = getDb();
    if(!db.isOpen())
        return 0;

    QSqlQuery query(db);
    query.prepare(R"(SELECT version FROM conversation_local WHERE peer_uid = :peer_uid AND session_type = :session_type)");
    query.bindValue(":peer_uid", peerUid);
    query.bindValue(":session_type", sessionType);
    if(query.exec() && query.next()) {
        return query.value("version").toULongLong();
    }
    return 0;
}

bool SqlWorker::upsertFriendApplySync(std::shared_ptr<FriendApplyInfo> applyInfo)
{
    if(applyInfo->fromUid.isEmpty() || applyInfo->toUid.isEmpty()) return false;

    QSqlDatabase db = getDb();
    if(!db.isOpen())
        return false;

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO
            friend_apply_local(from_uid, to_uid, status, greeting, apply_time, handle_time, version)
        VALUES
            (?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(from_uid, to_uid) DO UPDATE SET
            status = excluded.status,
            greeting = excluded.greeting,
            apply_time = excluded.apply_time,
            handle_time = excluded.handle_time,
            version = excluded.version
        WHERE excluded.version > version;
    )");

    query.bindValue(0, applyInfo->fromUid);
    query.bindValue(1, applyInfo->toUid);
    query.bindValue(2, applyInfo->status);
    query.bindValue(3, applyInfo->greeting);
    query.bindValue(4, applyInfo->applyTime);
    query.bindValue(5, applyInfo->handleTime);
    query.bindValue(6, applyInfo->version);

    return query.exec();
}

bool SqlWorker::updateAuthFriendSync(const QString &fromUid, const QString& toUid, int status, quint64 handleTime, quint64 version)
{
    if(fromUid.isEmpty() || toUid.isEmpty()) return false;

    QSqlDatabase db = getDb();
    if(!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE friend_apply_local
        SET status = ?, handle_time = ?, version = ?
        WHERE from_uid = ? AND to_uid = ?
    )");

    query.bindValue(0, status);
    query.bindValue(1, handleTime);
    query.bindValue(2, version);
    query.bindValue(3, fromUid);
    query.bindValue(4, toUid);

    if (!query.exec()) {
        qWarning() << "[SqlWorker] updateAuthFriendSync 失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool SqlWorker::updateFriendApplyVersionSync(const QString &fromUid, const QString& toUid, quint64 version)
{
    if(fromUid.isEmpty() || toUid.isEmpty()) return false;

    QSqlDatabase db = getDb();
    if(!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE friend_apply_local
        SET version = ?
        WHERE from_uid = ? AND to_uid = ?
    )");

    query.bindValue(0, version);
    query.bindValue(1, fromUid);
    query.bindValue(2, toUid);

    if (!query.exec()) {
        qWarning() << "[SqlWorker] updateFriendApplyVersionSync 失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool SqlWorker::updateFriendListVersionSync(const QString &friendUid, quint64 version)
{
    if(friendUid.isEmpty()) return false;

    QSqlDatabase db = getDb();
    if(!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE friend_list_local
        SET version = ?
        WHERE friend_uid = ?
    )");

    query.bindValue(0, version);
    query.bindValue(1, friendUid);

    if (!query.exec()) {
        qWarning() << "[SqlWorker] updateFriendListVersionSync 失败:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<FriendApplyInfo> SqlWorker::getAllFriendAppliesSync()
{
    QList<FriendApplyInfo> applies;
    QSqlDatabase db = getDb();
    if (!db.isOpen()) return applies;

    QSqlQuery query(db);
    QString sql = R"(
        SELECT
            a.from_uid, a.to_uid, a.status, a.greeting, a.apply_time, a.handle_time, a.version,
            u.username, u.email, u.avatar_url, u.version as user_version
        FROM friend_apply_local a
        LEFT JOIN user_info_local u ON u.uid = (CASE WHEN a.from_uid = :my_uid THEN a.to_uid ELSE a.from_uid END)
        ORDER BY MAX(a.apply_time, a.handle_time) DESC
    )";

    query.prepare(sql);
    query.bindValue(":my_uid", _currentUid); // 绑定当前登录用户的 UID

    if (query.exec()) {
        while (query.next()) {
            FriendApplyInfo info;
            info.fromUid = query.value("from_uid").toString();
            info.toUid = query.value("to_uid").toString();
            info.status = query.value("status").toInt();
            info.greeting = query.value("greeting").toString();
            info.applyTime = query.value("apply_time").toULongLong();
            info.handleTime = query.value("handle_time").toULongLong();
            info.version = query.value("version").toULongLong();
            applies.append(info);
        }
    }
    else {
        qWarning() << "[SqlWorker] 获取好友申请列表失败:" << query.lastError().text();
    }
    return applies;
}
