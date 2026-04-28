#include "chatmgr.h"
#include "Sqlite_Layer/sqlitemgr.h"
#include "Logic_Layer/logicmgr.h"
#include "ChatMsg_Layer/chatsession.h"
#include "UserData_Layer/usermgr.h"
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

ChatMgr::ChatMgr(QObject *parent): QObject(parent)
{
    _convModel = new ConversationsModel(this);
}

void ChatMgr::init()
{
    _convModel->init();

    connect(&SqliteMgr::getInstance(), &SqliteMgr::sig_initDb_success, this, [this] {
        qDebug() << "[ChatMgr] 数据库初始化成功，开始捞取未发送完成的遗留消息...";
        loadPendingMessages();
    });

    /// LogicMgr ---> ChatMgr
    connect(&LogicMgr::getInstance(), &LogicMgr::sig_recv_data_rsp, this, &ChatMgr::slot_recv_msg_rsp);

    connect(&LogicMgr::getInstance(), &LogicMgr::sig_reconnect_syncData_finished, this, [this]() {
        if (_flyingMsgs.isEmpty()) return;
        qDebug() << "[ChatMgr] 业务鉴权且同步完毕，开始严格按时序倾泻飞行消息，数量:" << _flyingMsgs.size();
        QList<PendingMsgNode*> listToResend = _flyingMsgs.values();
        // 统一恢复发送并重启定时器
        sendPendingMsgNodeList(listToResend);
    });

    /// TcpMgr ---> ChatMgr (监听网络状态)
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_net_state_changed, this, &ChatMgr::slot_net_state_changed);
}

ChatMgr &ChatMgr::getInstance()
{
    static ChatMgr instance;
    return instance;
}

ChatMgr *ChatMgr::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    ChatMgr* instance = &ChatMgr::getInstance();
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

void ChatMgr::shutDownAllFlyingMsgs()
{
    if(_flyingMsgs.isEmpty()) return;

    for(PendingMsgNode* pNode: _flyingMsgs) {
        if(pNode && pNode->timer->isActive())
            pNode->timer->stop();
    }
}

void ChatMgr::clearFlyingMsgs()
{
    for(auto it = _flyingMsgs.begin(); it != _flyingMsgs.end(); ++it) {
        const QString& clientMsgId = it.key();
        PendingMsgNode* pNode = it.value();
        if(pNode) {
            if(pNode->timer->isActive())
                pNode->timer->stop();
            pNode->timer->deleteLater();
            delete pNode;
            pNode = nullptr;
        }
    }
    _flyingMsgs.clear();
}

ChatSession* ChatMgr::getChatSession(const QString &targetUid, int sessionType)
{
    QString sessionKey = formatSessionKey(targetUid, sessionType);
    if(!_sessions.contains(sessionKey)) {
        _sessions[sessionKey] = new ChatSession(targetUid, sessionType, _convModel, this);
    }
    return _sessions[sessionKey];
}

// 用户打开会话时，加载历史记录模型
ChatMsgModel *ChatMgr::getChatModel(const QString &targetUid, int sessionType)
{
    if(targetUid.isEmpty()) return nullptr;

    QString sessionKey = formatSessionKey(targetUid, sessionType);
    ChatSession* session = getChatSession(targetUid, sessionType);

    if(!session->hasMsgModel()) {
        session->createMsgModel(); // 创建时先会拉取本地数据库数据（旧）
    }

    // 判断待拉取离线数据队列里有无该Model
    if(_syncTasks.contains(sessionKey)) {
        // 提高优先级，插队处理历史记录的拉取
        SyncTask& task = _syncTasks[sessionKey];
        qint64 now = QDateTime::currentMSecsSinceEpoch();

        // 幂等性校验：如果处于 Waiting，或者虽然是 Fetching 但已经超过了 5 秒（判定为上一包丢了）
        if(task.state == SyncState::Waiting || (now - task.lastReqTime > 5000)) {
            qDebug() << "[Sync] 触发最高优先级插队拉取:" << sessionKey;

            // 立刻更新状态并打上时间戳防重发
            task.state = SyncState::Fetching;
            task.lastReqTime = now;

            // 呼叫 LogicMgr 发送具体的网络请求
            LogicMgr::getInstance().sendPrioritySyncReq(targetUid, sessionType, task.startVersion, task.endVersion);
        }
    }

    // LRU 提权
    makeRecent(sessionKey);
    // 触发全局已读
    markAsRead(targetUid, sessionType);
    return session->getMsgModel();
}

void ChatMgr::makeRecent(const QString &sessionKey)
{
    if(_lruIters.contains(sessionKey)) {
        _lruQueue.erase(_lruIters[sessionKey]);
    }

    _lruQueue.emplace_front(sessionKey);
    _lruIters[sessionKey] = _lruQueue.begin();

    while(_lruQueue.size() > MAX_SESSION_CACHE) {
        QString oldestKey = _lruQueue.back();
        _lruQueue.pop_back();
        _lruIters.remove(oldestKey);

        if(_sessions.contains(oldestKey)) {
            _sessions[oldestKey]->destroyMsgModel();
        }
    }
}

void ChatMgr::markAsRead(const QString &peerUid, int sessionType)
{
    clearUnread(peerUid, sessionType);
}

ConversationsModel *ChatMgr::getConvModel() const
{
    return _convModel;
}

QString ChatMgr::getCurrentActiveSessionKey() const
{
    return _currentActiveSessionKey;
}

void ChatMgr::setCurrentActiveSession(const QString &uid, int sessionType)
{
    if(uid.isEmpty())
        _currentActiveSessionKey = "";
    else
        _currentActiveSessionKey = formatSessionKey(uid, sessionType);

}

int ChatMgr::getNetState() const
{
    return static_cast<int>(_netState);
}

void ChatMgr::setConversationList(const QList<ConversationInfo> &convList)
{
    _convModel->slot_resetConversations(convList);
}

void ChatMgr::clearConvList()
{
    _convModel->slot_resetConversations({});
}

void ChatMgr::clear()
{
    qDebug() << "[ChatMgr] 开始清理聊天缓存与视图模型...";

    // 停掉所有正在重传的消息定时器，清理内存
    clearFlyingMsgs();

    // 销毁所有的 ChatSession (包含其中的 ChatMsgModel)
    clearAllSessions();

    // 清空左侧会话列表的数据模型
    clearConvList();

    // 重置当前的活跃焦点
    _currentActiveSessionKey.clear();

    // 重置防刷屏限流器
    _rateLimiter.clear();

    qDebug() << "[ChatMgr] 清理完成。";
}

void ChatMgr::clearAllSessions()
{
    for (auto session : _sessions) {
        if (session) {
            session->deleteLater();
        }
    }
    _sessions.clear();
    _lruIters.clear();
    _lruQueue.clear();
}

/// 发送数据
void ChatMgr::sendChatMessage(const QString &targetUid, int sessionType, const QString &msgData)
{
    // 防刷屏拦截
    if (!_rateLimiter.canSend()) {
        qWarning() << "[RateLimiter] 发送过快，触发防刷屏保护！";
        // TODO: 可以抛出一个信号让UI弹窗提示 "发送太频繁，请稍后再试"
        return;
    }

    ChatSession* session = getChatSession(targetUid, sessionType);
    auto [msgInfo, convInfo] = session->handleSendMsg(msgData);

    qDebug() << "发送内容为：" << msgData;

    // 异步打包
    QtConcurrent::run([msgInfo, convInfo, session, this] {
        // 通知数据库落盘事务
        bool dbSuccess = SqliteMgr::getInstance().appendMessageTransactionSync({msgInfo}, convInfo);

        if (!dbSuccess) {
            qWarning() << "消息落盘失败，终止网络发送流程！";
            // 抛回主线程更新 UI 状态为“发送失败(红色感叹号)”
            QMetaObject::invokeMethod(this, [this, session, clientMsgId = msgInfo.clientMsgId, targetUid = msgInfo.peerUid, sessionType = msgInfo.sessionType]() {
                if(session->hasMsgModel())
                    session->getMsgModel()->updateMsgStatus(clientMsgId, 2);
            }, Qt::QueuedConnection);
            return;
        }

        // 网络层打包
        QByteArray jsonData = prepareMsgInfoJsonStr(msgInfo);

        // 投递主线程
        QMetaObject::invokeMethod(this, [this, msgInfo, jsonData] {
            // 创建节点
            PendingMsgNode* node = createPendingMsgNode(msgInfo, jsonData);

            // 插入字典
            _flyingMsgs.insert(msgInfo.clientMsgId, node);

            if(_netState == NetState::Connected) {
                // 发包
                TcpMgr::getInstance().sendData(ReqId::ID_CHAT_MSG_REQ, jsonData);
                // 启动定时器
                node->timer->start();
            }
        }, Qt::QueuedConnection);
    });
}

void ChatMgr::clearUnread(const QString &peerUid, int sessionType)
{
    // 后期如果要求同步进行，则用QtConCurrent::run包装
    // 修改本地数据库: 聊天记录表 (chat_history_local)
    SqliteMgr::getInstance().updateAllMsgReadStatus(peerUid, sessionType);

    // 修改本地数据库: 会话表 (conversation_local)
    // 内部 SQL: UPDATE conversation_local SET unread_count = 0 WHERE peer_uid = peerUid
    SqliteMgr::getInstance().clearUnreadCount(peerUid, sessionType);

    // 放到主线程队列确保 UI 更新安全
    QMetaObject::invokeMethod(this, [this, peerUid, sessionType]() {
        _convModel->clearUnread(peerUid, sessionType);
    }, Qt::QueuedConnection);


    // 修改可选 UI 模型: ChatMsgModel 内部的消息状态
    // 因为 ChatMsgModel 是 LRU 懒加载的，可能当前并没有实例化，所以必须判断是否存在。
    ChatSession* session = getChatSession(peerUid, sessionType);
    if (session && session->hasMsgModel()) {
        // 备忘：目前 ChatMsgModel 还没有暴露 IsReadRole。
        // TODO: 未来如果扩展了 IsReadRole（例如用于显示消息上的 "已读" 小绿标），在这里调用：
        // session->chatModel->markAllAsRead();
    }

    QJsonObject req;
    req["peerUid"] = peerUid;
    req["sessionType"] = sessionType;
    QByteArray data = QJsonDocument(req).toJson(QJsonDocument::Compact);
    TcpMgr::getInstance().sendData(ReqId::ID_CHAT_MSG_READ_REQ, data);
}

/// 接收聊天消息
void ChatMgr::slot_on_receive_chat_msg(const MsgInfo &msgInfo)
{
    // 拦截防踩踏：自己发给自己的消息，走专门的状态补全通道
    QString myUid = UserMgr::getInstance().uid();
    if (msgInfo.senderUid == myUid && msgInfo.peerUid == myUid) {
        qDebug() << "收到自聊消息的路由包，执行静默状态补全(不刷新UI气泡)";
        SqliteMgr::getInstance().updateSelfChatReceiveInfo(msgInfo);
        return;
    }

    QString sessionKey = formatSessionKey(msgInfo.peerUid, msgInfo.sessionType);
    ChatSession* session = getChatSession(msgInfo.peerUid, msgInfo.sessionType);

    // 判断当前所在界面是否刚好是这个会话
    bool isActive = (_currentActiveSessionKey == sessionKey);

    // C++17 结构化绑定：直接拿到原子快照
    auto [addedMsgs, convInfo] = session->handleReceiveMsg(msgInfo, isActive);

    if (!addedMsgs.isEmpty()) {
        // 直接拿着快照去落盘，彻底消灭中间状态被篡改的危险
        SqliteMgr::getInstance().appendMessageTransaction(addedMsgs, convInfo);
    }
}

void ChatMgr::slot_recv_msg_rsp(std::shared_ptr<MsgInfo> msgInfo, std::shared_ptr<ConversationInfo> convInfo)
{
    assert(msgInfo && convInfo);

    if(!_flyingMsgs.contains(msgInfo->clientMsgId)) return;

    auto node = _flyingMsgs.take(msgInfo->clientMsgId);
    if(!node) return;

    if(node->timer->isActive())
        node->timer->stop();
    node->timer->deleteLater();

    // 更新UI
    ChatMsgModel* chatModel = getChatSession(node->targetUid, node->sessionType)->getMsgModel();
    if(chatModel)
        chatModel->updateMsgStatus(msgInfo->clientMsgId, msgInfo->sendStatus);

    // 异步写入数据库
    SqliteMgr::getInstance().updateMsgStatusAndVersion(msgInfo, convInfo);

    delete node;
}

void ChatMgr::slot_net_state_changed(NetState state)
{
    if(_netState != state) {
        _netState = state;
        emit netStateChanged();
    }

    if(_netState == NetState::Connected) {
        qDebug() << "[ChatMgr] 物理网络已恢复，等待业务层静默鉴权就绪...";
    }
    else {
        // 网络断开，冻结所有定时器
        shutDownAllFlyingMsgs();
    }
}

void ChatMgr::sendPendingMsgNodeList(QList<PendingMsgNode *> list)
{
    std::sort(list.begin(), list.end(), [](PendingMsgNode* a, PendingMsgNode* b) {
        assert(a);
        assert(b);
        return a->createTime < b->createTime;
    });

    for (PendingMsgNode* pNode : list) {
        if (pNode) {
            pNode->retryCount = 0;
            pNode->timer->setInterval(2000);
            TcpMgr::getInstance().sendData(ReqId::ID_CHAT_MSG_REQ, pNode->networkPayload);
            pNode->timer->start();
        }
    }
}

// 初始化拉取
void ChatMgr::loadPendingMessages()
{
    // 从数据库同步拉取所有断电/杀后台遗留的“发送中”消息
    QList<MsgInfo> pendingMsgs = SqliteMgr::getInstance().getPendingMessagesSync();
    if (pendingMsgs.isEmpty()) return;

    qDebug() << "[ChatMgr] 发现" << pendingMsgs.size() << "条发送中的遗留消息，正在重新装载到飞行队列...";
    for(const MsgInfo& msgInfo: pendingMsgs) {
        // 序列化MsgInfo
        QByteArray jsonData = prepareMsgInfoJsonStr(msgInfo);
        // 创建重传节点
        PendingMsgNode* node = createPendingMsgNode(msgInfo, jsonData);
        // 插入缓存映射表
        _flyingMsgs.insert(msgInfo.clientMsgId, node);
        // 判断是否发送
        if (_netState == NetState::Connected) {
            TcpMgr::getInstance().sendData(ReqId::ID_CHAT_MSG_REQ, jsonData);
            node->timer->start();
        }
    }
}

QByteArray ChatMgr::prepareMsgInfoJsonStr(const MsgInfo& msgInfo)
{
    QJsonObject jsonObj;
    jsonObj["clientMsgId"] = msgInfo.clientMsgId;
    jsonObj["senderUid"]   = msgInfo.senderUid;
    jsonObj["targetUid"]   = msgInfo.peerUid;   // 接收方
    jsonObj["sessionType"] = msgInfo.sessionType;
    jsonObj["msgType"]     = msgInfo.msgType;
    jsonObj["msgData"]     = msgInfo.msgData;
    jsonObj["seqId"]       = QString::number(msgInfo.seqId); // 防止 uint64 溢出转字符串
    jsonObj["createTime"]  = QString::number(msgInfo.createTime);
    jsonObj["isRead"]      = msgInfo.isRead;
    QByteArray jsonData = QJsonDocument(jsonObj).toJson(QJsonDocument::Compact);
    return jsonData;
}

PendingMsgNode* ChatMgr::createPendingMsgNode(const MsgInfo& msgInfo, QByteArray jsonData)
{
    // 创建重传节点
    PendingMsgNode* node = new PendingMsgNode;
    node->clientMsgId = msgInfo.clientMsgId;
    node->targetUid = msgInfo.peerUid;
    node->sessionType = msgInfo.sessionType;
    node->createTime = msgInfo.createTime;
    node->networkPayload = jsonData;
    node->timer = new QTimer(this);
    // 配置定时器
    node->timer->setInterval(2000);
    node->timer->setSingleShot(true);
    // 创建信号
    connect(node->timer, &QTimer::timeout, this, [this, clientMsgId = msgInfo.clientMsgId, peerUid = msgInfo.peerUid, sessionType = msgInfo.sessionType]() {
        // 如果定时器在极端的网络切换微秒间隙触发了，只要当前没网，拒绝执行重传逻辑
        if (_netState != NetState::Connected) return;

        if (!_flyingMsgs.contains(clientMsgId)) return;
        auto pNode = _flyingMsgs[clientMsgId];
        pNode->retryCount++;
        if (pNode->retryCount > 3) {
            // 更新UI
            ChatSession* session = getChatSession(pNode->targetUid, pNode->sessionType);
            if(session->hasMsgModel())
                session->getMsgModel()->updateMsgStatus(clientMsgId, 2);

            // 异步写入数据库
            auto msgInfo = std::make_shared<MsgInfo>();
            msgInfo->clientMsgId = clientMsgId;
            msgInfo->msgId = "";
            msgInfo->sendStatus = 2;
            auto convInfo = std::make_shared<ConversationInfo>();
            convInfo->peerUid = peerUid;
            convInfo->sessionType = sessionType;
            convInfo->version = 0;
            SqliteMgr::getInstance().updateMsgStatusAndVersion(msgInfo, convInfo);

            // 删除待重传消息
            _flyingMsgs.remove(clientMsgId);
            pNode->timer->deleteLater();
            delete pNode;
        }
        else {
            pNode->timer->setInterval(pNode->timer->interval() * 2);
            pNode->timer->start();
            TcpMgr::getInstance().sendData(ReqId::ID_CHAT_MSG_REQ, pNode->networkPayload);
        }
    });

    return node;
}

// TODO: 引入多次优先级权衡机制
void ChatMgr::addSyncTask(const QString &peerUid, int sessionType, quint64 localVersion, quint64 endVersion)
{
    QString key = formatSessionKey(peerUid, sessionType);
    if(!_syncTasks.contains(key)) {
        _syncTasks[key] = {SyncState::Waiting, 0, localVersion, endVersion};
        qDebug() << "[Sync] 发现历史空洞，任务已压入队列:" << key;
    }
}

bool ChatMgr::isSessionSyncing(const QString &peerUid, int sessionType) const
{
    return _syncTasks.contains(formatSessionKey(peerUid, sessionType));
}

void ChatMgr::handleSyncMsgResult(const QString &peerUid, int sessionType, const QList<MsgInfo> &newMsgs, bool hasMore)
{
    QString key = formatSessionKey(peerUid, sessionType);
    ChatSession* session = getChatSession(peerUid, sessionType);

    // UI 倒灌与内存游标更新
    if (session && !newMsgs.isEmpty()) {
        QString myUid = UserMgr::getInstance().uid();
        quint64 maxPeerSeq = 0;

        for (const MsgInfo& msg : newMsgs) {
            // 我们只依靠“对方发来”的消息来推进 expectedRecvSeq
            if (msg.senderUid != myUid || sessionType == 2) {
                maxPeerSeq = qMax(msg.seqId, maxPeerSeq);
            }
        }

        ConversationInfo info = session->getConvInfo();
        // 如果这批离线消息里真的有对方的包，且让进度超前了，安全跃迁！
        if (maxPeerSeq >= info.expectedRecvSeq) {
            info.expectedRecvSeq = maxPeerSeq + 1;
            session->updateConvInfo(info);
            qDebug() << "[Sync_Debug] 离线包解析完毕！滑动窗口游标被真实数据安全推进至:" << info.expectedRecvSeq;
            SqliteMgr::getInstance().updateConvExpectedRecvSeq(peerUid, sessionType, info.expectedRecvSeq);
        }

        // 将历史记录安全倒灌进 UI（内部自动处理去重和批量刷新）
        if (session->hasMsgModel()) {
            session->getMsgModel()->insertGapMessages(newMsgs);
        }
    }

    // 状态机推进：有界向左逼近
    if (_syncTasks.contains(key)) {
        if (hasMore && !newMsgs.isEmpty()) {
            _syncTasks[key].endVersion = newMsgs.first().msgId.toULongLong();
            _syncTasks[key].state = SyncState::Waiting;
            processNextSyncTask();
        }
        else {
            qDebug() << "[Sync] 会话" << key << "的历史记录补洞任务已彻底完成！";
            completeSyncTask(peerUid, sessionType);
        }
    }
}

void ChatMgr::completeSyncTask(const QString &peerUid, int sessionType)
{
    QString key = formatSessionKey(peerUid, sessionType);
    if (!_syncTasks.contains(key)) return;

    // 清理已完成的任务
    _syncTasks.remove(key);

    if (_syncTasks.isEmpty()) {
        qDebug() << "[ChatMgr] 所有会话的离线历史记录拉取完毕！";
        emit sig_sync_history_finished();
    }
    else {
        // 还有其他会话在排队，去捞下一个
        processNextSyncTask(); // 继续拉取下一个
    }
}

void ChatMgr::checkAndStartSyncTasks()
{
    if (_syncTasks.isEmpty()) {
        qDebug() << "[ChatMgr] 没有需要拉取的历史记录，直接通关！";
        emit sig_sync_history_finished();
    }
    else {
        qDebug() << "[ChatMgr] 开始处理历史记录拉取队列，共" << _syncTasks.size() << "个会话";
        processNextSyncTask(); // 开始拉取第一个
    }
}

void ChatMgr::processNextSyncTask()
{
    int fetchingCount = 0;
    QString taskToRun = "";
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 遍历状态机，统计正在拉取的数量，顺便清理超时死锁
    for (auto it = _syncTasks.begin(); it != _syncTasks.end(); ++it) {
        if (it.value().state == SyncState::Fetching) {
            if (now - it.value().lastReqTime > 5000) {
                qDebug() << "[Sync] 任务超时复位:" << it.key();
                it.value().state = SyncState::Waiting;
            }
            else {
                fetchingCount++;
            }
        }
    }

    // 并发控制：如果当前同时有 >= 2 个会话在拉数据，就先排队，别把网络堵死
    if (fetchingCount >= 2) return;

    // 找出一个处于 Waiting 的任务去执行
    for (auto it = _syncTasks.begin(); it != _syncTasks.end(); ++it) {
        if (it.value().state == SyncState::Waiting) {
            taskToRun = it.key();
            break;
        }
    }

    // 发射网络请求
    if (!taskToRun.isEmpty()) {
        SyncTask& task = _syncTasks[taskToRun];
        task.state = SyncState::Fetching;
        task.lastReqTime = now;

        qDebug() << "历史记录起始游标: " << task.startVersion << " 结尾游标: " << task.endVersion;
        QStringList parts = taskToRun.split("_");
        LogicMgr::getInstance().sendPrioritySyncReq(parts[0], parts[1].toInt(), task.startVersion, task.endVersion);
    }
}
