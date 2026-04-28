#include "logicmgr.h"
#include "Net_Layer/tcpmgr.h"
#include "Http_Layer/httpmgr.h"
#include "UserData_Layer/usermgr.h"
#include "ChatMsg_Layer/chatmgr.h"
#include "Sqlite_Layer/sqlitemgr.h"
#include "Logic_Layer/actiondebouncer.h"
#include <QtConcurrent/QtConcurrentRun>

struct SyncTaskParams {
    QString peerUid;
    int sessionType;
    quint64 localVer;
    quint64 endVer;
};

LogicMgr::LogicMgr(QObject *parent)
    : QObject{parent}
{
    _searchModel = new SearchModel(this);
    _sendRateLimiter = std::make_shared<SendRateLimiter>();
    _actionDebouncer = std::make_shared<ActionDebouncer>();
    _friendApplyModel = new FriendApplyModel(this);
}

// C++单例
LogicMgr &LogicMgr::getInstance()
{
    static LogicMgr instance;
    return instance;
}

// QML单例（内部调用的是C++单例，所以全局共享一个静态对象）
LogicMgr* LogicMgr::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine) {
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    LogicMgr* instance = &getInstance();
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
    return instance;
}

SearchModel *LogicMgr::getSearchModel()
{
    return _searchModel;
}

FriendApplyModel *LogicMgr::getFriendApplyModel()
{
    return _friendApplyModel;
}

void LogicMgr::syncDataReq()
{
    // 获取本地好友表、会话表、好友申请表的最大版本号
    quint64 maxFriendVer = SqliteMgr::getInstance().getMaxFriendVersion();
    quint64 maxConvVer = SqliteMgr::getInstance().getMaxConversationVersion();
    quint64 maxApplyVer = SqliteMgr::getInstance().getMaxFriendApplyVersion();

    // 组装增强请求Json
    QJsonObject reqObj;
    reqObj["friendVersion"] = QString::number(maxFriendVer);
    reqObj["convVersion"] = QString::number(maxConvVer);
    reqObj["friendApplyVersion"] = QString::number(maxApplyVer);

    // 发包，向服务器数据库拉取增量信息
    QByteArray reqData = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);
    TcpMgr::getInstance().sendData(ReqId::ID_SYNC_INIT_REQ, reqData);
}

QList<std::shared_ptr<UserInfo> > LogicMgr::mergeUserInfoList(const QList<std::shared_ptr<UserInfo>>& list1, const QList<std::shared_ptr<UserInfo>>& list2)
{

    QHash<QString, std::shared_ptr<UserInfo>> hash;
    for(auto userInfo: list1) {
        if (userInfo)
            hash[userInfo->_uid] = userInfo;
    }

    for(auto userInfo: list2) {
        if (!userInfo) continue;
        QString uid = userInfo->_uid;

        if(hash.contains(uid)) {
            if(hash[uid]->_version > userInfo->_version || hash[uid]->_updateTime > userInfo->_updateTime)
                continue;
        }
        // 插入陌生人，或用高版本覆盖旧数据
        hash.insert(uid, userInfo);
    }

    return hash.values();
}

void LogicMgr::init()
{
    _friendApplyModel->init();

    connect(&HttpMgr::getInstance(), &HttpMgr::sig_showTip_changed, this, &LogicMgr::sig_showTip_changed);

    connect(&HttpMgr::getInstance(), &HttpMgr::sig_reg_mod_success, this, &LogicMgr::sig_reg_mod_success);

    connect(&HttpMgr::getInstance(), &HttpMgr::sig_loginShowTip_changed, this, &LogicMgr::sig_loginShowTip_changed);

    connect(&SqliteMgr::getInstance(), &SqliteMgr::sig_initDb_success, this, [this]{
        qDebug() << "本地数据库就绪，准备向服务器拉取增量数据...";
        QtConcurrent::run([this](){
            syncDataReq();
        });
    });

    // 重连成功，重新模拟登录，增量取数据
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_reconnect_success, this, [this](bool success) {
        if(!success) return;

        QString uid = UserMgr::getInstance().uid();
        QString token = UserMgr::getInstance().token();

        qDebug() << "[LogicMgr] 捕获重连成功信号，触发静默鉴权恢复...";
        QJsonObject jsonObj;
        jsonObj["uid"] = uid;
        jsonObj["token"] = token;
        QByteArray jsonData = QJsonDocument(jsonObj).toJson(QJsonDocument::Compact);

        TcpMgr::getInstance().sendData(ReqId::ID_CHAT_LOGIN, jsonData);
    });

    connect(&TcpMgr::getInstance(), &TcpMgr::sig_net_state_changed, this, [this](NetState state) {
        emit sig_net_reconnecting(state == NetState::Connecting);
    });

    connect(&ChatMgr::getInstance(), &ChatMgr::sig_sync_history_finished, this, [this]() {
        if (!_isReconnecting.load(std::memory_order_acquire)) {
            // 通知 QML 隐藏 Loading 界面，正式进入聊天主界面
            emit sig_switch_chatDialog();
        }
        else
            finishReconnectSync();
    });

    initHandlers();
}

void LogicMgr::clear()
{
    _isReconnecting.store(false, std::memory_order_relaxed);
    clearSearch();
    clearFriendApplyModel();
}

/// Http相关模块功能
void LogicMgr::send_varifyCode(QString email)
{
    HttpMgr::getInstance().slot_send_varifyCode(email);
}

void LogicMgr::confirmBtn_clicked(QString user, QString email, QString passwd, QString confirm, QString varifyCode)
{
    HttpMgr::getInstance().slot_confirmBtn_clicked(user, email, passwd, confirm, varifyCode);
}

void LogicMgr::reset_confirm_clicked(QString user, QString email, QString newPassword, QString newConfirm, QString varifyCode)
{
    HttpMgr::getInstance().slot_reset_confirm_clicked(user, email, newPassword, newConfirm, varifyCode);
}

void LogicMgr::loginBtn_clicked(QString user, QString password)
{
    HttpMgr::getInstance().slot_loginBtn_clicked(user, password);
}

/// 调用处理函数
void LogicMgr::dispatchNetDataAsync(ReqId id, QByteArray data)
{
    if (_isReconnecting.load(std::memory_order_acquire)) {
        // 白名单：只有登录回包和同步拉取回包允许通行
        if (id != ReqId::ID_CHAT_LOGIN_RSP &&
            id != ReqId::ID_SYNC_INIT_RSP &&
            id != ReqId::ID_SYNC_MSG_RSP)
        {
            // 其他所有包（比如 ID_CHAT_MSG_NOTIFY, ID_AUTH_FRIEND_NOTIFY）全部拦截进蓄水池
            qDebug() << "[LogicMgr] 重连同步期屏障生效，拦截实时信令暂存，ReqId:" << static_cast<int>(id);
            _reconnectNotifyQueue.enqueue({id, data});
            return;
        }
    }

    if(!_handlers.contains(id)) {
        qWarning() << "[LogicMgr] 未知的请求 ID:" << static_cast<int>(id);
        return;
    }

    auto future = QtConcurrent::run([this, id, data] {
        _handlers[id](data);
    });
}

void LogicMgr::sendPrioritySyncReq(const QString &peerUid, int sessionType, quint64 startVersion, quint64 endVersion)
{
    // 异步执行，不卡住主线程 UI
    QtConcurrent::run([=]() {
        QJsonObject req;
        req["peerUid"] = peerUid;
        req["sessionType"] = sessionType;
        req["startVersion"] = QString::number(startVersion);
        req["endVersion"] = QString::number(endVersion);

        // 将高优插队请求打包发送
        QByteArray jsonData = QJsonDocument(req).toJson(QJsonDocument::Compact);
        TcpMgr::getInstance().sendData(ReqId::ID_SYNC_MSG_REQ, jsonData);
    });
}

void LogicMgr::finishReconnectSync()
{
    qDebug() << "[LogicMgr] 增量同步与离线拉取全部完毕，拆除重连屏障！";
    _isReconnecting = false;

    // 倾泻别人的实时来信（先处理接收）
    while(!_reconnectNotifyQueue.isEmpty()) {
        auto item = _reconnectNotifyQueue.dequeue();
        qDebug() << "[LogicMgr] 开闸放水：重新分发被拦截的实时信令，ReqId:" << static_cast<int>(item.first);
        dispatchNetDataAsync(item.first, item.second);
    }

    // 发射信号，触发 ChatMgr 倾泻自己的飞行消息（后处理自己的发送）
    emit sig_reconnect_syncData_finished();
}

/// 注册处理函数
void LogicMgr::initHandlers()
{
    // 注册：第一次登录时的回包逻辑
    _handlers.insert(ReqId::ID_CHAT_LOGIN_RSP, [this](QByteArray data)
    {
        // 将接收的QByteArray转换成QJsonDocument
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) {
            qDebug() << "当前执行登录回包逻辑：Failed to create QJsonDocument Or QJsonDoucment is not an Object" << Qt::endl;
            return;
        }
        QJsonObject jsonObj = doc.object();
        if(!jsonObj.contains("error")) {    // 说明Json回包没有按格式传输
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "Login Failed, err is Json Parse Err " << err << Qt::endl;
            emit HttpMgr::getInstance().sig_login_failed(err);
            return;
        }

        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS) {
            qDebug() << "Login Failed, err is " << error << Qt::endl;
            emit HttpMgr::getInstance().sig_login_failed(error);
            return;
        }

        QString name = jsonObj["name"].toString();
        QString uid = jsonObj["uid"].toString();
        QString token = jsonObj["token"].toString();
        QString email = jsonObj["email"].toString();

        /// 下面是本地存储用户信息
        QMetaObject::invokeMethod(this, [this, name, uid, token, email]{
            // 不相等代表此时UserMgr的uid还没赋值，第一次登录
            if(UserMgr::getInstance().uid() != uid) {
                _isReconnecting.store(false, std::memory_order_relaxed);
                UserMgr::getInstance().setUsername(name);
                UserMgr::getInstance().setUid(uid);
                UserMgr::getInstance().setToken(token);
                UserMgr::getInstance().setEmail(email);
                qDebug() << "Login Success, the uid is " << UserMgr::getInstance().uid() << " , the token is " << UserMgr::getInstance().token() << Qt::endl;

                // 发起本地数据初始化
                SqliteMgr::getInstance().initDatabase(uid);
            }
            else {
                _isReconnecting.store(true, std::memory_order_relaxed);
                qDebug() << "[LogicMgr] 断线重连静默登录成功！准备拉取离线期间的增量数据...";
                QtConcurrent::run([this]() {
                    syncDataReq();
                });
            }
        }, Qt::QueuedConnection);
    });

    // 注册："好友列表拉取请求"响应的回包逻辑
    _handlers.insert(ReqId::ID_GET_FRIEND_LIST_RSP, [this](QByteArray data) {
        qDebug() << "Received Friend List RSP";

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject())
            return;

        QJsonObject jsonObj = doc.object();
        if(!jsonObj.contains("error"))
            return;

        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS) {
            qDebug() << "Failed to get friend list, err is: " << error;
            return;
        }

        if(!jsonObj["friendList"].isArray()) {
            qDebug() << "friendList is not an array!";
            return;
        }

        QJsonArray friendArray = jsonObj["friendList"].toArray();
        QList<std::shared_ptr<UserInfo>> friendList;

        for(int i = 0; i < friendArray.size(); ++i) {
            QJsonObject userObj = friendArray[i].toObject();

            auto user = std::make_shared<UserInfo>();
            user->_uid = userObj["uid"].toString();
            user->_username = userObj["name"].toString();
            user->_email = userObj["email"].toString();
            friendList.append(user);
        }

        QMetaObject::invokeMethod(this, [this, friendList]{
            UserMgr::getInstance().loadAllUserCache(friendList);
            // 切换到聊天主界面
            emit sig_switch_chatDialog();
            qDebug() << "Successfully loaded" << friendList.size() << "friends to local cache.";
        }, Qt::QueuedConnection);

    });

    // 注册：查询请求时的回包逻辑
    _handlers.insert(ReqId::ID_SEARCH_USER_RSP, [this](QByteArray data) {
        // 将接收的QByteArray转换成QJsonDocument
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) {
            qDebug() << "Failed to create QJsonDocument Or QJsonDoucment is not an Object" << Qt::endl;
            return;
        }
        QJsonObject jsonObj = doc.object();
        if(!jsonObj.contains("error")) {    // 说明Json回包没有按格式传输
            int err = ErrorCodes::ERR_JSON;
            qDebug() << "Search Failed, err is Json Parse Err " << err << Qt::endl;
            emit sig_search_failed(err);
            return;
        }

        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS) {
            // 抛回主线程发射信号并清空搜索
            QMetaObject::invokeMethod(this, [this, error]() {
                emit sig_search_failed(error);
                _searchModel->clearSearch();
            });
            return;
        }

        QJsonArray searchArray = jsonObj["searchList"].toArray();
        QList<std::shared_ptr<UserInfo>> resultList;

        for (int i = 0; i < searchArray.size(); ++i) {
            QJsonObject userObj = searchArray[i].toObject();

            auto user = std::make_shared<UserInfo>();
            user->_uid = userObj["uid"].toString();
            user->_username = userObj["name"].toString();
            user->_email = userObj["email"].toString();
            user->_version = userObj["version"].toString().toULongLong();
            resultList.append(user);
        }

        // 把拼装好的数据一把塞给 SearchModel
        QMetaObject::invokeMethod(this, [this, resultList] {
            _searchModel->setSearchResults(resultList);
            qDebug() << "Search success! Found" << resultList.size() << "users.";
        }, Qt::QueuedConnection);
    });

    // 注册：发送方"添加好友"响应的回包逻辑
    _handlers.insert(ReqId::ID_ADD_FRIEND_RSP, [this](QByteArray data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) {
            qDebug() << "Failed to create QJsonDocument Or QJsonDoucment is not an Object" << Qt::endl;
            return;
        }

        QJsonObject jsonObj = doc.object();
        if(!jsonObj.contains("error") || jsonObj["error"].toInt() != ErrorCodes::SUCCESS)
            return;

        QString targetUid = jsonObj["targetUid"].toString();
        quint64 applyVersion = jsonObj["applyVersion"].toString().toULongLong();
        QString myUid = UserMgr::getInstance().uid();

        // 停止防抖
        QMetaObject::invokeMethod(this, [this, targetUid]{
            QString actionId = "addFriend_" + targetUid;
            if(_actionDebouncer->isTimerActive(actionId))
                _actionDebouncer->reset(actionId);
        }, Qt::QueuedConnection);


        // 底层落盘：更新 SQLite 里的 Version
        SqliteMgr::getInstance().updateFriendApplyVersionSync(myUid, targetUid, applyVersion);

        // 内存同步：获取当前 Model 里的申请记录，贴上 Version 后扔回去
        QMetaObject::invokeMethod(this, [this, targetUid, applyVersion]() {
            auto applyInfo = _friendApplyModel->getFriendApplyInfo(targetUid);
            if (applyInfo) {
                applyInfo->version = applyVersion;
                // 静默刷新，不置顶也不弹红点
                _friendApplyModel->upsertApply(*applyInfo, FriendApplyModel::ActionReceive);
            }
        });

    });

    // 注册：接收方"完成好友添加"响应的回包逻辑
    _handlers.insert(ReqId::ID_AUTH_FRIEND_RSP, [this](QByteArray data) {
        // 将接收的QByteArray转换成QJsonDocument
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) {
            qDebug() << "Failed to create QJsonDocument Or QJsonDoucment is not an Object" << Qt::endl;
            return;
        }

        QJsonObject jsonObj = doc.object();
        if(!jsonObj.contains("error") || jsonObj["error"].toInt() != ErrorCodes::SUCCESS)
            return;

        QString action = jsonObj["action"].toString();
        QString applicantUid = jsonObj["applicantUid"].toString();
        quint64 applyVersion = jsonObj["applyVersion"].toString().toULongLong();
        QString myUid = UserMgr::getInstance().uid();

        // 停止防抖
        QMetaObject::invokeMethod(this, [this, applicantUid]{
            QString actionId = "authFriend_" + applicantUid;
            if(_actionDebouncer->isTimerActive(actionId))
                _actionDebouncer->reset(actionId);
        }, Qt::QueuedConnection);

        SqliteMgr::getInstance().updateFriendApplyVersionSync(applicantUid, myUid, applyVersion);

        // 如果是“同意”则更新friend_list_local的版本
        if(action == "agree") {
            SqliteMgr::getInstance().updateFriendListVersionSync(applicantUid, jsonObj["friendVersion"].toString().toULongLong());
        }

        QMetaObject::invokeMethod(this, [this, applicantUid, applyVersion]() {
            auto applyInfo = _friendApplyModel->getFriendApplyInfo(applicantUid);
            if (applyInfo) {
                applyInfo->version = applyVersion;
                // 静默刷新，不置顶也不弹红点
                _friendApplyModel->upsertApply(*applyInfo, FriendApplyModel::ActionReceive);
            }
        });
    });

    // 注册：发送方"添加好友"通知的响应逻辑
    _handlers.insert(ReqId::ID_ADD_FRIEND_NOTIFY, [this](QByteArray data) {
        // 将接收的QByteArray转换成QJsonDocument
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) {
            qDebug() << "Failed to create QJsonDocument Or QJsonDoucment is not an Object" << Qt::endl;
            return;
        }

        QJsonObject jsonObj = doc.object();
        QString fromUid = jsonObj["fromUid"].toString();
        QString fromUsername = jsonObj["fromUsername"].toString();
        QString fromUserEmail = jsonObj["fromUserEmail"].toString();
        QString fromUserVersion = jsonObj["fromUserVersion"].toString();
        QString applyTime = jsonObj["applyTime"].toString();
        QString greeting = jsonObj["greeting"].toString();
        QString applyVersion = jsonObj["applyVersion"].toString();

        std::shared_ptr<UserInfo> userInfo = std::make_shared<UserInfo>();
        userInfo->_uid = fromUid;
        userInfo->_username = fromUsername;
        userInfo->_email = fromUserEmail;
        userInfo->_version = fromUserVersion.toULongLong();
        userInfo->_updateTime = applyTime.toLongLong();
        userInfo->_status = 1;
        userInfo->_isFriend = false;

        // 缓存本地user_info_local 表
        if(SqliteMgr::getInstance().addOrUpdateUserInfoSync(userInfo)) {
            UserMgr::getInstance().updateUserInfo(userInfo);
        }

        auto applyInfo = std::make_shared<FriendApplyInfo>();
        applyInfo->fromUid = fromUid;
        applyInfo->toUid = UserMgr::getInstance().uid();
        applyInfo->status = 0;
        applyInfo->greeting = greeting;
        applyInfo->applyTime = applyTime.toLongLong();
        applyInfo->handleTime = 0;
        applyInfo->version = applyVersion.toULongLong();

        // 缓存本地friend_apply_local 表:
        SqliteMgr::getInstance().upsertFriendApplySync(applyInfo);
        QMetaObject::invokeMethod(this, [this, applyInfo] {
            // 更新本地 UI
            _friendApplyModel->upsertApply(*applyInfo, FriendApplyModel::ActionReceive);
        });
    });

    // 注册：发送方：接收到服务器"完成好友添加"通知的响应逻辑
    _handlers.insert(ReqId::ID_AUTH_FRIEND_NOTIFY, [this](QByteArray data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) {
            qDebug() << "Failed to create QJsonDocument Or QJsonDoucment is not an Object" << Qt::endl;
            return;
        }

        QJsonObject jsonObj = doc.object();
        QString action = jsonObj["action"].toString();
        QString friendUid = jsonObj["friendUid"].toString();
        QString friendName = jsonObj["friendName"].toString();
        QString friendEmail = jsonObj["friendEmail"].toString();
        quint64 handleTime = jsonObj["handleTime"].toString().toULongLong();
        quint64 applyVersion = jsonObj["applyVersion"].toString().toULongLong();
        QString myUid = UserMgr::getInstance().uid();

        int status = (action == "agree") ? 1 : 2;

        // 更新friend_apply_local 表
        SqliteMgr::getInstance().updateAuthFriendSync(myUid, friendUid, status, handleTime, applyVersion);

        auto applyInfo = _friendApplyModel->getFriendApplyInfo(friendUid);
        applyInfo->status = status;
        applyInfo->handleTime = handleTime;
        applyInfo->version = applyVersion;

        QMetaObject::invokeMethod(this, [this, applyInfo] {
            // 更新好友请求模型UI
            _friendApplyModel->upsertApply(*applyInfo, FriendApplyModel::ActionReceive);
        });

        if(action == "agree") {
            auto userInfo = std::make_shared<UserInfo>();
            userInfo->_uid = friendUid;
            userInfo->_username = friendName;
            userInfo->_email = friendEmail;
            userInfo->_createTime = handleTime;
            userInfo->_updateTime = handleTime;
            userInfo->_version = jsonObj["friendVersion"].toString().toULongLong();
            userInfo->_isFriend = true;
            userInfo->_status = 1;
            // 更新friend_list_local
            SqliteMgr::getInstance().addOrUpdateFriendSync(userInfo);

            QMetaObject::invokeMethod(this, [this, userInfo, jsonObj] {
                // 更新好友列表UI
                UserMgr::getInstance().updateUserInfo(userInfo);
                emit sig_add_friend_success(jsonObj);
            }, Qt::QueuedConnection);
        }
        else
            // (可选) 对方拒绝了您，可以发个另外的信号提示一下
            qDebug() << "Your friend request was rejected by: " << jsonObj["friendUid"].toString();
    });

    // 注册：收到服务器发送成功的回包逻辑
    _handlers.insert(ReqId::ID_CHAT_MSG_RSP, [this](QByteArray data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) return;

        QJsonObject jsonObj = doc.object();
        QString clientMsgId = jsonObj["clientMsgId"].toString();
        QString msgId = jsonObj["msgId"].toString();
        QString version = jsonObj["version"].toString();
        QString peerUid = jsonObj["peerUid"].toString();
        int sessionType = jsonObj["sessionType"].toInt();

        int error = jsonObj["error"].toInt();

        auto msgInfo = std::make_shared<MsgInfo>();
        msgInfo->clientMsgId = clientMsgId;
        msgInfo->msgId = msgId;
        msgInfo->sendStatus = error == ErrorCodes::SUCCESS ? 1 : 2;

        auto convInfo = std::make_shared<ConversationInfo>();
        convInfo->peerUid = peerUid;
        convInfo->sessionType = sessionType;
        convInfo->version = version.toULongLong();

        emit sig_recv_data_rsp(msgInfo, convInfo);
    });

    // 注册：接收聊天消息通知的响应逻辑
    _handlers.insert(ReqId::ID_CHAT_MSG_NOTIFY, [this](QByteArray data){
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) return;

        QJsonObject jsonObj = doc.object();
        MsgInfo msgInfo;
        msgInfo.clientMsgId = getClientMsgId();
        msgInfo.msgId = jsonObj["msgId"].toString();
        msgInfo.senderUid = jsonObj["senderUid"].toString();
        msgInfo.peerUid = msgInfo.senderUid; // 单聊对端就是发送方
        msgInfo.msgData = jsonObj["msgData"].toString();
        msgInfo.msgType = jsonObj["msgType"].toInt();
        msgInfo.seqId = jsonObj["seqId"].toString().toULongLong();
        msgInfo.createTime = jsonObj["createTime"].toString().toLongLong();
        msgInfo.isRead = jsonObj["isRead"].toInt();
        msgInfo.isSelf = false;
        msgInfo.sendStatus = 1;
        msgInfo.sessionType = jsonObj["sessionType"].toInt();

        // 立即向服务器回复 ACK 回执
        QJsonObject ackObj;
        ackObj["msgId"] = msgInfo.msgId;
        ackObj["clientMsgId"] = msgInfo.clientMsgId;
        QByteArray ackData = QJsonDocument(ackObj).toJson(QJsonDocument::Compact);
        TcpMgr::getInstance().sendData(ReqId::ID_CHAT_MSG_NOTIFY_ACK, ackData);

        QMetaObject::invokeMethod(&ChatMgr::getInstance(), [this, msgInfo]() {
            ChatMgr::getInstance().slot_on_receive_chat_msg(msgInfo);
            emit sig_receive_chat_msg(QString("%1_%2").arg(msgInfo.senderUid).arg(msgInfo.sessionType));
        });
    });

    // 注册：接收补洞响应
    _handlers.insert(ReqId::ID_MSG_REPAIR_RSP, [this](QByteArray data){
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) return;

        QJsonObject obj = doc.object();
        if (obj["error"].toInt() != ErrorCodes::SUCCESS) {
            qWarning() << "[LogicMgr] 拉取空洞消息失败！";
            return;
        }

        QJsonArray array = obj["msgList"].toArray();
        for(int i = 0; i < array.size(); ++i) {
            QJsonObject jsonObj = array[i].toObject();
            MsgInfo msgInfo;
            msgInfo.clientMsgId = getClientMsgId();
            msgInfo.msgId = jsonObj["msgId"].toString();
            msgInfo.senderUid = jsonObj["senderUid"].toString();
            msgInfo.peerUid = msgInfo.senderUid; // 单聊对端就是发送方
            msgInfo.msgData = jsonObj["msgData"].toString();
            msgInfo.msgType = jsonObj["msgType"].toInt();
            msgInfo.seqId = jsonObj["seqId"].toString().toULongLong();
            msgInfo.createTime = jsonObj["createTime"].toString().toLongLong();
            msgInfo.isRead = jsonObj["isRead"].toInt();
            msgInfo.isSelf = false;
            msgInfo.sendStatus = 1;
            msgInfo.sessionType = jsonObj["sessionType"].toInt();

            QMetaObject::invokeMethod(&ChatMgr::getInstance(), [this, msgInfo]() {
                ChatMgr::getInstance().slot_on_receive_chat_msg(msgInfo);
                emit sig_receive_chat_msg(QString("%1_%2").arg(msgInfo.senderUid).arg(msgInfo.sessionType));
            });
        }

        // TODO:回执
    });

    // 注册：增量数据拉取的响应逻辑
    _handlers.insert(ReqId::ID_SYNC_INIT_RSP, [this](QByteArray data){
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) return;
        QJsonObject jsonObj = doc.object();
        if(jsonObj["error"].toInt() != ErrorCodes::SUCCESS) return;

        QString myUid = UserMgr::getInstance().uid();

        // 提取Json数组
        QList<std::shared_ptr<UserInfo>> deltaFriends = friendListFromJsonArray(jsonObj["deltaFriends"].toArray());
        QList<ConversationInfo> deltaConvs = convListFromJsonArray(jsonObj["deltaConvs"].toArray());

        // 解析增量好友申请 (分离出 申请单 本身和 申请人 的兜底用户资料)
        QJsonArray applyArray = jsonObj["deltaApplies"].toArray();
        QList<std::shared_ptr<FriendApplyInfo>> deltaApplies;
        QList<std::shared_ptr<UserInfo>> deltaApplyUsers;

        for (int i = 0; i < applyArray.size(); ++i) {
            QJsonObject applyObj = applyArray[i].toObject();
            int status = applyObj["status"].toInt();
            QString fromUid = applyObj["fromUid"].toString();
            QString toUid = applyObj["toUid"].toString();

            // 组装申请单涉及的用户资料 (存入 user_info_local)
            auto userInfo = std::make_shared<UserInfo>();
            userInfo->_uid = (fromUid == myUid) ? toUid : fromUid;
            userInfo->_username = applyObj["targetUsername"].toString();
            userInfo->_email = applyObj["targetEmail"].toString();
            userInfo->_version = applyObj["targetVersion"].toString().toULongLong();
            userInfo->_updateTime = applyObj["handleTime"].toString().toLongLong();
            userInfo->_createTime = applyObj["handleTime"].toString().toLongLong();
            userInfo->_status = 1;
            userInfo->_isFriend = (status == 1);
            deltaApplyUsers.append(userInfo);

            // 组装申请单本身 (存入 friend_apply_local)
            auto applyInfo = std::make_shared<FriendApplyInfo>();
            applyInfo->fromUid = fromUid;
            applyInfo->toUid = toUid;
            applyInfo->status = status;
            applyInfo->greeting = applyObj["greeting"].toString();
            applyInfo->applyTime = applyObj["applyTime"].toString().toLongLong();
            applyInfo->handleTime = applyObj["handleTime"].toString().toLongLong();
            applyInfo->version = applyObj["applyVersion"].toString().toULongLong();
            deltaApplies.append(applyInfo);
        }

        QList<SyncTaskParams> pendingSyncTasks;
        for (const auto& conv : deltaConvs) {
            // 去本地查这个会话当前的 Version
            quint64 localVer = SqliteMgr::getInstance().getLocalMaxVersionSync(conv.peerUid, conv.sessionType);
            quint64 endVer = conv.version + 1;

            if (endVer > localVer) {
                pendingSyncTasks.append({conv.peerUid, conv.sessionType, localVer, endVer});
                qDebug() << "[Sync_Debug] 锁定会话" << conv.peerUid << "的拉取区间: (" << localVer << "," << endVer << "]";
            }
        }

        // 同步本地数据库
        SqliteMgr::getInstance().syncDeltaFriends(deltaFriends);
        SqliteMgr::getInstance().syncDeltaConversations(deltaConvs);

        for (int i = 0; i < deltaApplies.size(); ++i) {
            // 状态非1代表不是好友，此时才需要兜底用户资料。是好友的话 deltaFriends 已经插过了。
            if (deltaApplies[i]->status != 1) {
                SqliteMgr::getInstance().addOrUpdateUserInfoSync(deltaApplyUsers[i]);
            }
            SqliteMgr::getInstance().upsertFriendApplySync(deltaApplies[i]);
        }

        auto fullFriendList = SqliteMgr::getInstance().getAllFriendsSync();
        auto fullConvList = SqliteMgr::getInstance().getAllConversationsSync();
        auto fullApplyList = SqliteMgr::getInstance().getAllFriendAppliesSync();

        QMetaObject::invokeMethod(this, [this, fullFriendList, fullConvList, fullApplyList, deltaApplyUsers, pendingSyncTasks]() {
            auto mergedUsers = mergeUserInfoList(fullFriendList, deltaApplyUsers);
            // 统一重置 UserMgr (涵盖好友列表 + 新的陌生人申请者资料)
            UserMgr::getInstance().loadAllUserCache(mergedUsers);

            // 统一重置 ConversationsModel
            ChatMgr::getInstance().setConversationList(fullConvList);

            // 统一重置 FriendApplyModel
            _friendApplyModel->loadApplies(fullApplyList);

            // 将阶段二快照的任务，统一喂给 ChatMgr
            for (const auto& task : pendingSyncTasks) {
                ChatMgr::getInstance().addSyncTask(task.peerUid, task.sessionType, task.localVer, task.endVer);
            }

            ChatMgr::getInstance().checkAndStartSyncTasks();

        }, Qt::QueuedConnection);
    });

    _handlers.insert(ReqId::ID_CHAT_MSG_READ_RSP, [this](QByteArray data) {
        // 将接收的QByteArray转换成QJsonDocument
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) {
            qDebug() << "Failed to create QJsonDocument Or QJsonDoucment is not an Object" << Qt::endl;
            return;
        }

        QJsonObject jsonObj = doc.object();
        if(!jsonObj.contains("error") || jsonObj["error"].toInt() != ErrorCodes::SUCCESS)
            return;

        // TODO：接收到服务器的确定已读请求
        qDebug() << "确认服务器已接收广播已读请求";
    });

    _handlers.insert(ReqId::ID_CHAT_MSG_READ_NOTIFY, [this](QByteArray data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) return;

        QJsonObject jsonObj = doc.object();
        QString readerUid = jsonObj["readerUid"].toString();
        int sessionType = jsonObj["sessionType"].toInt();

        // 仅修改本地数据库：chat_history_local
        SqliteMgr::getInstance().updateAllMsgReadStatus(readerUid, sessionType);

        // 仅更新可选的 UI 模型：ChatMsgModel
        QMetaObject::invokeMethod(&ChatMgr::getInstance(), [readerUid, sessionType]() {
            // 在 ChatMgr 中尝试获取该会话
            auto session = ChatMgr::getInstance().getChatSession(readerUid, sessionType);

            // 可选模型判定：如果当前对方正打开着聊天框 (chatModel存在)，则让气泡底下的 "未读" 变成 "已读"
            if (session && session->hasMsgModel()) {
                // 【注】：未来在 chatmsgmodel.h 里增加 markAllAsRead() 方法，
                // 遍历内部 m_messages，把 is_sender == true 的 is_read 设为 1，并 emit dataChanged
            }

            qDebug() << "[LogicMgr] 收到对方已读通知，已静默更新对方:" << readerUid << "的消息已读状态。";
        }, Qt::QueuedConnection);
    });

    _handlers.insert(ReqId::ID_SYNC_MSG_RSP, [this](QByteArray data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) return;
        QJsonObject jsonObj = doc.object();

        QString peerUid = jsonObj["peerUid"].toString();
        int sessionType = jsonObj["sessionType"].toInt();
        QString myUid = UserMgr::getInstance().uid();

        // 关键字段：服务器告诉你是否还有未拉取完的数据
        bool hasMore = jsonObj["hasMore"].toInt();
        QJsonArray msgArray = jsonObj["msgList"].toArray();

        qDebug() << "拉取到的离线数据数量： " << msgArray.size();
        // 拉取离线信息，注意要转化视角，服务器是保存的无状态的
        QList<MsgInfo> newMsgs;
        for (int i = 0; i < msgArray.size(); ++i) {
            QJsonObject obj = msgArray[i].toObject();
            QString senderUid = obj["senderUid"].toString(); // 服务器传来的发送者
            QString peerUid = obj["peerUid"].toString(); // 服务器传来的接收者

            MsgInfo msg;
            msg.clientMsgId = obj["clientMsgId"].toString();
            msg.msgId = obj["msgId"].toString();
            msg.seqId = obj["seqId"].toString().toULongLong();
            msg.createTime = obj["createTime"].toString().toULongLong();
            msg.peerUid = (senderUid == myUid ? peerUid : senderUid); // 比如如果是B给A发送消息，senderUid就是B,但是peerUid是A，所以A拉取信息要转换成A的视角
            msg.senderUid = senderUid;
            msg.sessionType = obj["sessionType"].toInt();
            msg.msgData = obj["msgData"].toString();
            msg.msgType = obj["msgType"].toInt();
            msg.isSelf = (msg.senderUid == myUid);
            msg.sendStatus = 1;
            msg.isRead = obj["isRead"].toInt();
            newMsgs.append(msg);
        }

        qDebug() << "[Sync_Debug] 收到服务器下发的历史记录，共" << newMsgs.size() << "条，hasMore:" << hasMore;
        if(!newMsgs.isEmpty()) {
            qDebug() << "[Sync_Debug] 第一条 MsgId:" << newMsgs.first().msgId << "最后一条 MsgId:" << newMsgs.last().msgId;
        }

        // 强一致性落盘：只要有数据，必须先写进本地 SQLite
        // 这里会阻塞当前工作线程，直到 SQLite 落盘完成，绝不会和主线程冲突
        if (!newMsgs.isEmpty()) {
            qDebug() << "准备落盘离线数据";
            bool dbSuccess = SqliteMgr::getInstance().insertGapMessagesSync(peerUid, sessionType, newMsgs);
            if (!dbSuccess) {
                qWarning() << "[LogicMgr] 历史记录落盘严重失败，为了防止死锁将强行中断任务！";
                hasMore = false; // 落盘失败，强行将 hasMore 置为 false，让状态机抛弃该任务
            }
        }

        // 数据确实安全落盘后，切回主线程驱动模型和状态机
        QMetaObject::invokeMethod(&ChatMgr::getInstance(), [peerUid, sessionType, newMsgs, hasMore]() {
            ChatMgr::getInstance().handleSyncMsgResult(peerUid, sessionType, newMsgs, hasMore);
        }, Qt::QueuedConnection);
    });

    _handlers.insert(ReqId::ID_HEARTBEAT_RSP, [this](QByteArray data) {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if(doc.isNull() || !doc.isObject()) return;
        QJsonObject jsonObj = doc.object();

        qDebug() << "收到服务器的ID_HEARTBEAT_RSP心跳响应回包，TODO:更新服务器最后活跃数据等等依赖参数";
    });
}

/// QML调用接口
void LogicMgr::searchUser(QString text)
{
    QtConcurrent::run([=]{
        QJsonObject jsonObj;
        jsonObj["targetUsername"] = text;
        jsonObj["targetEmail"] = text;
        QByteArray jsonData = QJsonDocument(jsonObj).toJson(QJsonDocument::Compact);
        TcpMgr::getInstance().sendData(ID_SEARCH_USER_REQ, jsonData);
    });
}

void LogicMgr::clearSearch()
{
    _searchModel->clearSearch();
}

void LogicMgr::clearFriendApplyModel()
{
    _friendApplyModel->clear();
}

void LogicMgr::sendAddFriendRequest(const QJsonObject &reqData)
{
    QString actionId = "addFriend_" + reqData["targetUid"].toString();
    if(!_actionDebouncer->tryExecute(actionId, 2000)) {
        qDebug() << "操作太频繁，已防抖拦截:" << actionId;
        return;
    }

    // 基本数据
    QString targetUid = reqData["targetUid"].toString();
    QString greeting = reqData["greeting"].toString();
    QString remark = reqData["remark"].toString();
    qint64 applyTime = QDateTime::currentMSecsSinceEpoch();
    QString myUid = UserMgr::getInstance().uid();

    // 获取SearchModel里的对应数据
    auto targetUser = _searchModel->getUserInfo(targetUid);
    if(!targetUser) return;

    // 构造用户缓存资料
    auto user = std::make_shared<UserInfo>();
    user->_uid = targetUid;
    user->_username = targetUser->_username;
    user->_email = targetUser->_email;
    user->_avatarUrl = targetUser->_avatarUrl;
    user->_version = targetUser->_version; // 继承服务器下发的 UserVersion
    user->_updateTime = applyTime;
    user->_isFriend = false;
    user->_status = 1;

    // 构造好友申请单 (ApplyVersion 暂时为 0，等 RSP)
    auto applyInfo = std::make_shared<FriendApplyInfo>();
    applyInfo->fromUid = UserMgr::getInstance().uid();
    applyInfo->toUid = targetUid;
    applyInfo->status = 0;
    applyInfo->greeting = greeting;
    applyInfo->applyTime = applyTime;
    applyInfo->handleTime = 0;
    applyInfo->version = 0;

    QtConcurrent::run([=, this]() {
        // 同步落盘 user_info_local 表
        if(SqliteMgr::getInstance().addOrUpdateUserInfoSync(user)) {

            // 更新users列表
            QMetaObject::invokeMethod(this, [=, this]{
                UserMgr::getInstance().updateUserInfo(user);
            }, Qt::QueuedConnection);
        }

        // 同步落盘到 friend_apply_local 表
        if(SqliteMgr::getInstance().upsertFriendApplySync(applyInfo)) {

            // 乐观更新 UI 模型 (自动置顶)
            QMetaObject::invokeMethod(this, [this, applyInfo] {
                _friendApplyModel->upsertApply(*applyInfo, FriendApplyModel::ActionSend);
            }, Qt::QueuedConnection);
        }

        // TODO: 后期优化 -> 引入统一抽象的网络请求队列，所有网络请求必须支持 [超时重试 + ACK删除] 机制;
        QJsonObject req;
        req["targetUid"] = applyInfo->toUid;
        req["greeting"] = applyInfo->greeting;
        req["applyTime"] = QString::number(applyInfo->applyTime);
        QByteArray jsonData = QJsonDocument(req).toJson(QJsonDocument::Compact);
        TcpMgr::getInstance().sendData(ID_ADD_FRIEND_REQ, jsonData);
    });
}

void LogicMgr::sendAuthFriendRequest(const QJsonObject &reqData)
{
    // 申请人UID + 同意/拒绝
    QString applicantUid = reqData["applicantUid"].toString();
    QString action = reqData["action"].toString();

    // 防抖
    QString actionId = "authFriend_" + applicantUid;
    if(!_actionDebouncer->tryExecute(actionId, 2000))
    {
        qDebug() << "防抖ing... 请求被拦截！";
        return;
    }

    // 自身UID + 行为状态 + 处理时间
    QString myUid = UserMgr::getInstance().uid();
    int status = (action == "agree") ? 1 : 2;
    qint64 handleTime = QDateTime::currentMSecsSinceEpoch();

    // 好友表用户信息
    auto oldApply = _friendApplyModel->getFriendApplyInfo(applicantUid);
    if (!oldApply) {
        qWarning() << "[LogicMgr] 致命错误：内存中找不到 UID 为" << applicantUid << "的好友申请记录，已强行中断同意流程！";
        return;
    }

    FriendApplyInfo newApply = *oldApply;
    newApply.status = status;
    newApply.handleTime = handleTime;
    newApply.version = 0;

    // 用户表目标用户信息
    auto targetUser = UserMgr::getInstance().getUserInfo(applicantUid);
    if(!targetUser) {
        qDebug() << "users表内没有该用户: " << applicantUid;
    }
    std::shared_ptr<UserInfo> newUserInfo = std::make_shared<UserInfo>();
    if (targetUser)
        UserMgr::getInstance().cloneUserInfoPtr(newUserInfo, targetUser);

    if (action == "agree") {
        newUserInfo->_isFriend = true;
        newUserInfo->_status = 1;
        newUserInfo->_createTime = handleTime;
    }

    QtConcurrent::run([=]() {
        // 落盘申请表
        SqliteMgr::getInstance().updateAuthFriendSync(applicantUid, myUid, status, handleTime, 0);

        if(action == "agree")
            // 落盘好友列表
            SqliteMgr::getInstance().addOrUpdateFriendSync(newUserInfo);

        // 投递主线程UI
        QMetaObject::invokeMethod(this, [this, newApply, action, applicantUid, handleTime, newUserInfo] {
            // 更新好友请求模型
            _friendApplyModel->upsertApply(newApply, FriendApplyModel::ActionSend);
            // 更新好友列表UI
            UserMgr::getInstance().updateUserInfo(newUserInfo);
        }, Qt::QueuedConnection);

        // 打包发送网络请求
        // TODO: 后期优化 -> 引入统一抽象的网络请求队列，所有网络请求必须支持 [超时重试 + ACK删除] 机制
        QJsonObject req;
        req["applicantUid"] = applicantUid;
        req["action"] = action;
        req["handleTime"] = QString::number(handleTime);
        QByteArray jsonData = QJsonDocument(req).toJson(QJsonDocument::Compact);
        TcpMgr::getInstance().sendData(ID_AUTH_FRIEND_REQ, jsonData);
    });
}

void LogicMgr::logoutAndClean()
{
    qDebug() << "[LogicMgr] 开始执行全局注销与清理流程...";

    // 网络层斩断
    TcpMgr::getInstance().disconnectServer();

    // 数据层落盘并关闭
    SqliteMgr::getInstance().closeDatabase();

    // 各大业务模块自我清理
    UserMgr::getInstance().clear();
    ChatMgr::getInstance().clear();

    // LogicMgr 自身的清理
    this->clear();
}

QList<std::shared_ptr<UserInfo>> LogicMgr::friendListFromJsonArray(const QJsonArray& array)
{
    QList<std::shared_ptr<UserInfo>> userList;
    for(int i = 0; i < array.size(); ++i) {
        QJsonObject obj = array[i].toObject();
        auto userInfo = std::make_shared<UserInfo>();
        userInfo->_uid = obj["uid"].toString();
        userInfo->_username = obj["username"].toString();
        userInfo->_email = obj["email"].toString();
        userInfo->_avatarUrl = obj["avatarUrl"].toString();
        userInfo->_status = obj["status"].toInt();
        userInfo->_version = obj["version"].toString().toULongLong();
        userInfo->_createTime = obj["createTime"].toString().toULongLong();
        userInfo->_isFriend = true;
        userList.append(userInfo);
    }
    return userList;
}

QList<ConversationInfo> LogicMgr::convListFromJsonArray(const QJsonArray &array)
{
    QList<ConversationInfo> convList;
    for(int i = 0; i < array.size(); ++i) {
        QJsonObject obj = array[i].toObject();
        ConversationInfo convInfo;
        convInfo.peerUid = obj["peerUid"].toString();
        convInfo.sessionType = obj["sessionType"].toInt();
        convInfo.lastMsgSenderUid = obj["lastMsgSenderUid"].toString();
        convInfo.lastMsgContent = obj["lastMsgContent"].toString();
        convInfo.lastMsgType = obj["lastMsgType"].toInt();
        convInfo.lastSeqId = obj["lastSeqId"].toString().toULongLong();
        convInfo.unreadCount = obj["unreadCount"].toInt();
        convInfo.updateTime = obj["updateTime"].toString().toULongLong();
        convInfo.version = obj["version"].toString().toULongLong();
        convList.append(convInfo);
    }
    return convList;
}

