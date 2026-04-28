#ifndef LOGICMGR_H
#define LOGICMGR_H

#include <QObject>
#include "global.h"
#include "Model_Layer/searchmodel.h"
#include "Model_Layer/friendapplymodel.h"


class SendRateLimiter;
class ActionDebouncer;

class LogicMgr : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(SearchModel* searchModel READ getSearchModel CONSTANT)
    Q_PROPERTY(FriendApplyModel* friendApplyModel READ getFriendApplyModel CONSTANT)
public:
    static LogicMgr& getInstance();

    static LogicMgr* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    LogicMgr(const LogicMgr&) = delete;

    LogicMgr& operator=(const LogicMgr&) = delete;

    /// 注册处理函数
    void initHandlers(); // 注册网络包处理函数

    SearchModel* getSearchModel();

    FriendApplyModel* getFriendApplyModel();

    void init();

    void clear();

/// QML调用接口
    /// 下面是特殊按钮的交互槽
    Q_INVOKABLE void send_varifyCode(QString email);

    Q_INVOKABLE void confirmBtn_clicked(QString user, QString email, QString passwd, QString confirm, QString varifyCode);

    Q_INVOKABLE void reset_confirm_clicked(QString user, QString email, QString newPassword, QString newConfirm, QString varifyCode);

    Q_INVOKABLE void loginBtn_clicked(QString user, QString password);

    // 查找用户
    Q_INVOKABLE void searchUser(QString text);

    // 清理查找列表
    Q_INVOKABLE void clearSearch();

    // 发送添加好友请求
    Q_INVOKABLE void sendAddFriendRequest(const QJsonObject& reqData);

    // 应答好友添加请求
    Q_INVOKABLE void sendAuthFriendRequest(const QJsonObject& reqData);

    // 登出并清理数据
    Q_INVOKABLE void logoutAndClean();

    // 清理好友请求列表
    Q_INVOKABLE void clearFriendApplyModel();

/// 公共API
public:
    // 接收 TcpMgr 传来的原始字节包
    void dispatchNetDataAsync(ReqId id, QByteArray data);

    // 触发高优单聊/群聊历史记录拉取
    void sendPrioritySyncReq(const QString& peerUid, int sessionType, quint64 startVersion, quint64 endVersion);

    // 统一结束重连处理
    void finishReconnectSync();

signals:
    // UI 相关的信号 (抛给 QML 或其他管理器)
    void sig_showTip_changed(QString res, bool status);

    void sig_loginShowTip_changed(QString res, bool status);

    void sig_reg_mod_success();

    void sig_login_success(); // 登录成功的信号

    void sig_switch_chatDialog(); // 登录界面跳转聊天主界面的信号

    void sig_search_failed(int err); // 查找用户失败的信号

    void sig_friend_apply(QJsonObject applyInfo); // 接收好友请求的信号

    void sig_auth_rsp(QJsonObject rspInfo); // 接收方 B 收到的处理结果回包

    void sig_add_friend_success(QJsonObject notifyInfo); // 发送方 A 收到的对方已同意/拒绝的跨服通知

    // 网络相关的信号
    void sig_receive_chat_msg(QString key); // 收到新的聊天消息信号

    void sig_recv_data_rsp(std::shared_ptr<MsgInfo> msgInfo, std::shared_ptr<ConversationInfo> convInfo);

    void sig_net_reconnecting(bool active);

    void sig_reconnect_syncData_finished();

private:
    explicit LogicMgr(QObject *parent = nullptr);

    SearchModel* _searchModel{};

    FriendApplyModel* _friendApplyModel{};

    // 路由分发器
    QHash<ReqId, std::function<void(QByteArray data)>> _handlers;

    // 区分首登还是重连
    std::atomic<bool> _isReconnecting{false};

    // 重连真空期的实时信令蓄水池
    QQueue<QPair<ReqId, QByteArray>> _reconnectNotifyQueue;

    // 截流器
    std::shared_ptr<SendRateLimiter> _sendRateLimiter{};

    // 防抖器
    std::shared_ptr<ActionDebouncer> _actionDebouncer{};

private:
    void syncDataReq();

    QList<std::shared_ptr<UserInfo>> mergeUserInfoList(const QList<std::shared_ptr<UserInfo>>& list1, const QList<std::shared_ptr<UserInfo>>& list2);

    QList<std::shared_ptr<UserInfo>> friendListFromJsonArray(const QJsonArray& array);

    QList<ConversationInfo> convListFromJsonArray(const QJsonArray& array);
};

#endif // LOGICMGR_H
