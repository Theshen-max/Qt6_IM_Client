#ifndef HTTPMGR_H
#define HTTPMGR_H

#include "global.h"

class HttpMgr: public QObject
{
    Q_OBJECT

public:
    static HttpMgr& getInstance();

    HttpMgr(const HttpMgr&) = delete;

    HttpMgr& operator=(const HttpMgr&) = delete;

    void init();

private:
    // 构造函数
    HttpMgr(QObject* parent = nullptr);

    void checkLoadComplete(); // 同步屏障检查函数

private:
    // 成员函数
    void postHttpReq(QUrl url, QJsonObject json, ReqId reqId, Modules mod);

    void initHandlers();

    // 成员变量
    QNetworkAccessManager _manager;

    // ReqId与回调函数的映射
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;

    // 作为第一次登录的缓存
    QString _tcpToken;

    uint64_t _tcpUid;

public slots:
    void slot_http_finish(ReqId id, QString res, ErrorCodes err, Modules mod);

    void slot_reg_finish(ReqId id, QString res);

    void slot_reset_finish(ReqId id, QString res);

    void slot_login_request_finish(ReqId id, QString res);

    void slot_tcpCon_success(bool successful);

    void slot_login_failed(int err);

    void slot_login_success();

    /// 下面是特殊按钮的交互槽
    void slot_send_varifyCode(QString email);

    void slot_confirmBtn_clicked(QString user, QString email, QString passwd, QString confirm, QString varifyCode);

    void slot_reset_confirm_clicked(QString user, QString email, QString newPassword, QString newConfirm, QString varifyCode);

    void slot_loginBtn_clicked(QString user, QString password);

signals:
    void sig_http_finish(ReqId id, QString res, ErrorCodes err, Modules mod);

    // 注册模块的相关通知信号
    void sig_reg_mod_finish(ReqId id, QString res);

    void sig_reg_mod_success();

    // 重置密码模块的相关通知信号
    void sig_reset_mod_finish(ReqId id, QString res);

    // 登录模块的相关通知信号
    void sig_login_request_mod_finish(ReqId id, QString res);

    void sig_connect_tcp(ServerInfo info);

    void sig_login_failed(int error);

    /// 下面是特殊按钮的交互信号
    // errTip的文本变化信号
    void sig_showTip_changed(QString res, bool status);

    // 登录界面的文本变化信号
    void sig_loginShowTip_changed(QString res, bool status);

    void sig_loginBtn_enabled(bool enabled);
};

#endif // HTTPMGR_H
