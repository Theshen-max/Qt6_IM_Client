#ifndef USERMGR_H
#define USERMGR_H

#include "global.h"
#include "Model_Layer/searchmodel.h"
#include "Model_Layer/friendlistmodel.h"

class UserMgr: public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON // 注册为 QML 单例

    // 注册属性，让 QML 可以读取、修改并监听变化
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(QString uid READ uid WRITE setUid NOTIFY uidChanged)
    Q_PROPERTY(QString token READ token WRITE setToken NOTIFY tokenChanged)
    Q_PROPERTY(QString email READ email WRITE setEmail NOTIFY emailChanged)
    Q_PROPERTY(QString curApplicantUid READ curApplicantUid WRITE setCurApplicantUid NOTIFY curApplicantUidChanged)
    Q_PROPERTY(QString curApplicantUsername READ curApplicantUsername WRITE setCurApplicantUsername NOTIFY curApplicantUsernameChanged)
    Q_PROPERTY(QString curApplicantUserEmail READ curApplicantUserEmail WRITE setCurApplicantUserEmail NOTIFY curApplicantUserEmailChanged)
    Q_PROPERTY(SearchModel* searchModel READ getSearchModel CONSTANT)
    Q_PROPERTY(FriendListModel* friendModel READ getFriendModel CONSTANT)

public:
    UserMgr(const UserMgr&) = delete;

    UserMgr& operator=(const UserMgr&) = delete;

    static UserMgr& getInstance();

    static UserMgr* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    void setUsername(QString name);

    void setUid(QString uid);

    void setToken(QString token);

    void setEmail(QString email);

    void setCurApplicantUid(QString applicantUid);

    void setCurApplicantUsername(QString applicantUsername);

    void setCurApplicantUserEmail(QString applicantUserEmail);

    QString username();

    QString uid();

    QString token();

    QString email();

    QString curApplicantUid();

    QString curApplicantUsername();

    QString curApplicantUserEmail();

    QHash<QString, std::shared_ptr<UserInfo>> getAllUsers() const;

    SearchModel* getSearchModel() const;

    FriendListModel* getFriendModel() const;

    std::shared_ptr<const UserInfo> getUserInfo(const QString& uid);

    void init();

    void clear();

    void cloneUserInfoPtr(std::shared_ptr<UserInfo> newUserInfo, std::shared_ptr<const UserInfo> oldUserInfo);

    void loadAllUserCache(const QList<std::shared_ptr<UserInfo>>& userList);

signals:
    void usernameChanged(QString username);

    void uidChanged(QString uid);

    void tokenChanged(QString token);

    void emailChanged(QString email);

    void curApplicantUidChanged(QString applicantUid);

    void curApplicantUsernameChanged(QString applicantUsername);

    void curApplicantUserEmailChanged(QString applicantUserEmail);

    void sig_userInfo_changed(const QString& uid);

    void sig_allUserCache_loaded();

public:
    Q_INVOKABLE void search(QString text);

    Q_INVOKABLE void clearSearch();

    Q_INVOKABLE void updateUserInfo(std::shared_ptr<UserInfo> userInfo);

public slots:
    void slot_on_userInfo_loaded(std::shared_ptr<UserInfo> userInfo);

private:
    UserMgr(QObject* parent = nullptr);

    QString _username;

    QString _token;

    QString _uid;

    QString _email;

    QString _curApplicantUid;

    QString _curApplicantUsername;

    QString _curApplicantUserEmail;

    QHash<QString, std::shared_ptr<UserInfo>> _users; // key: uid, value: uid绑定的UserInfo

    SearchModel* _searchModel;

    FriendListModel* _friendListModel;
};

#endif // USERMGR_H
