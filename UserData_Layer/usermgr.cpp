#include "usermgr.h"
#include "Sqlite_Layer/sqlitemgr.h"

UserMgr::UserMgr(QObject* parent): QObject(parent)
{
    _searchModel = new SearchModel(this);
    _searchModel->setSourceData(_users);
    _friendListModel = new FriendListModel(this);
}

UserMgr &UserMgr::getInstance()
{
    static UserMgr instance;
    return instance;
}

UserMgr *UserMgr::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    UserMgr* instance = &(UserMgr::getInstance());

    // 剥夺 QML 对该对象的所有权，防止垃圾回收干掉静态变量
    QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);

    return instance;
}

void UserMgr::setUsername(QString username)
{
    if(username != _username) {
        _username = username;
        emit usernameChanged(_username);
    }
}

void UserMgr::setUid(QString uid)
{
    if(uid != _uid) {
        _uid = uid;
        emit uidChanged(_uid);
    }
}

void UserMgr::setToken(QString token)
{
    if(token != _token)
    {
        _token = token;
        emit tokenChanged(_token);
    }
}

void UserMgr::setEmail(QString email)
{
    if(email != _email)
    {
        _email = email;
        emit emailChanged(_email);
    }
}

void UserMgr::setCurApplicantUid(QString applicantUid)
{
    if(applicantUid != _curApplicantUid) {
        _curApplicantUid = applicantUid;
        emit curApplicantUidChanged(_curApplicantUid);
    }
}

void UserMgr::setCurApplicantUsername(QString applicantUsername)
{
    if(applicantUsername != _curApplicantUsername) {
        _curApplicantUsername = applicantUsername;
        emit curApplicantUsernameChanged(_curApplicantUsername);
    }
}

void UserMgr::setCurApplicantUserEmail(QString applicantUserEmail)
{
    if(applicantUserEmail != _curApplicantUserEmail) {
        _curApplicantUserEmail = applicantUserEmail;
        emit curApplicantUserEmailChanged(_curApplicantUserEmail);
    }
}

QString UserMgr::username()
{
    return _username;
}

QString UserMgr::uid()
{
    return _uid;
}

QString UserMgr::token()
{
    return _token;
}

QString UserMgr::email()
{
    return _email;
}

QString UserMgr::curApplicantUid()
{
    return _curApplicantUid;
}

QString UserMgr::curApplicantUsername()
{
    return _curApplicantUsername;
}

QString UserMgr::curApplicantUserEmail()
{
    return _curApplicantUserEmail;
}

QHash<QString, std::shared_ptr<UserInfo> > UserMgr::getAllUsers() const
{
    return _users;
}

SearchModel *UserMgr::getSearchModel() const
{
    return _searchModel;
}

FriendListModel *UserMgr::getFriendModel() const
{
    return _friendListModel;
}

void UserMgr::init()
{
    _friendListModel->init();

    connect(&SqliteMgr::getInstance(), &SqliteMgr::sig_rspUserInfo, this, &UserMgr::slot_on_userInfo_loaded);
}

void UserMgr::clear()
{
    qDebug() << "[UserMgr] 开始清理用户数据及伴生模型...";

    // 清空当前用户的基础状态
    setUid("");
    setUsername("");
    setToken("");
    setEmail("");

    // 清空申请人的临时状态
    setCurApplicantUid("");
    setCurApplicantUsername("");
    setCurApplicantUserEmail("");

    // 清空全局好友与陌生人缓存
    _users.clear();

    // 级联清空麾下的 UI 数据模型
    if (_searchModel) {
        _searchModel->clear();
    }
    if (_friendListModel) {
        _friendListModel->clear();
    }

    qDebug() << "[UserMgr] 数据清理完毕。";
}

void UserMgr::cloneUserInfoPtr(std::shared_ptr<UserInfo> newUserInfo, std::shared_ptr<const UserInfo> oldUserInfo)
{
    Q_ASSERT(newUserInfo && oldUserInfo);

    // 只覆盖非空、非零的有效数据
    if(!oldUserInfo->_uid.isEmpty()) newUserInfo->_uid = oldUserInfo->_uid;
    if (!oldUserInfo->_username.isEmpty()) newUserInfo->_username = oldUserInfo->_username;
    if (!oldUserInfo->_email.isEmpty()) newUserInfo->_email = oldUserInfo->_email;
    if (!oldUserInfo->_avatarUrl.isEmpty()) newUserInfo->_avatarUrl = oldUserInfo->_avatarUrl;
    if (!oldUserInfo->_remark.isEmpty()) newUserInfo->_remark = oldUserInfo->_remark;

    // 核心状态必须覆盖
    newUserInfo->_status = oldUserInfo->_status;
    newUserInfo->_isFriend = oldUserInfo->_isFriend;

    // 保护绝对单调递增的值：版本号和时间戳只能变大，绝不能被 0 覆盖
    if (oldUserInfo->_version > newUserInfo->_version) newUserInfo->_version = oldUserInfo->_version;
    if (oldUserInfo->_createTime > newUserInfo->_createTime) newUserInfo->_createTime = oldUserInfo->_createTime;
    if (oldUserInfo->_updateTime > newUserInfo->_updateTime) newUserInfo->_updateTime = oldUserInfo->_updateTime;
}

std::shared_ptr<const UserInfo> UserMgr::getUserInfo(const QString &uid)
{
    if(_users.contains(uid))
        return _users.value(uid);

    // 传入空占位，避免缓存穿透
    _users.insert(uid, nullptr);

    // 异步向数据库发起请求
    SqliteMgr::getInstance().reqUserInfo(uid);

    // 先显示默认头像和默认 UID 占位
    return nullptr;
}

void UserMgr::search(QString text)
{
    if(_users.isEmpty()) return;

    QString str = text.trimmed();
    if(str.isEmpty()) {
        clearSearch();
        return;
    }

    _searchModel->search(str);
}

void UserMgr::clearSearch()
{
    if(_users.isEmpty()) return;
    _searchModel->clearSearch();
}

void UserMgr::loadAllUserCache(const QList<std::shared_ptr<UserInfo> > &userList)
{
    for (const auto& userInfo : userList) {
        if (!userInfo || userInfo->_uid.isEmpty()) continue;

        QString uid = userInfo->_uid;
        if (_users.contains(uid) && _users.value(uid) != nullptr) {
            auto existing = _users.value(uid);
            cloneUserInfoPtr(existing, userInfo);
        }
        else
            _users.insert(uid, userInfo);
    }

    emit sig_allUserCache_loaded();
}

void UserMgr::updateUserInfo(std::shared_ptr<UserInfo> userInfo)
{
    if (!userInfo || userInfo->_uid.isEmpty()) return;

    QString uid = userInfo->_uid;
    // 如果存在且不为空，就原地修改
    if(_users.contains(uid) && _users.value(uid) != nullptr) {
        auto existing = _users.value(uid);
        cloneUserInfoPtr(existing, userInfo);
    }
    else
        // 否则插入
        _users.insert(uid, userInfo);

    emit sig_userInfo_changed(uid);
}

void UserMgr::slot_on_userInfo_loaded(std::shared_ptr<UserInfo> userInfo)
{
    updateUserInfo(userInfo);
}
