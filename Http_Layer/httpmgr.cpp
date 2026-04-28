#include "Http_Layer/httpmgr.h"
#include "Net_Layer/tcpmgr.h"
#include "Logic_Layer/logicmgr.h"
#include <QTimer>

HttpMgr::HttpMgr(QObject* parent): QObject(parent)
{

}

HttpMgr &HttpMgr::getInstance()
{
    static HttpMgr instance;
    return instance;
}

void HttpMgr::init()
{
    connect(this, &HttpMgr::sig_http_finish, this, &HttpMgr::slot_http_finish);
    connect(this, &HttpMgr::sig_reg_mod_finish, this, &HttpMgr::slot_reg_finish);
    connect(this, &HttpMgr::sig_reset_mod_finish, this, &HttpMgr::slot_reset_finish);
    connect(this, &HttpMgr::sig_login_request_mod_finish, this, &HttpMgr::slot_login_request_finish);
    connect(this, &HttpMgr::sig_connect_tcp, &TcpMgr::getInstance(), &TcpMgr::slot_tcp_connect);
    connect(this, &HttpMgr::sig_login_failed, this, &HttpMgr::slot_login_failed);
    connect(&TcpMgr::getInstance(), &TcpMgr::sig_connect_success, this, &HttpMgr::slot_tcpCon_success);
    connect(&LogicMgr::getInstance(), &LogicMgr::sig_login_success, this, &HttpMgr::slot_login_success);
    initHandlers();
}

// 初始化事件处理器
void HttpMgr::initHandlers()
{
    // 注册获取验证码的回包逻辑
    _handlers.insert(ReqId::ID_GET_VARIFY_CODE, [this](const QJsonObject& jsonObj) {
        // 获取JSON数据包的错误码
        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS) {
            emit sig_showTip_changed(tr("参数错误"), false);
            return;
        }
        emit sig_showTip_changed(tr("验证码已经发送到邮箱，注意查收"), true);
    });

    // 注册用户注册的回包逻辑
    _handlers.insert(ReqId::ID_REG_USER, [this](const QJsonObject& jsonObj) {
        int err = jsonObj["error"].toInt();
        if(err == ErrorCodes::UserExited) {
            emit sig_showTip_changed(tr("用户已存在"), false);
            return;
        }
        else if(err == ErrorCodes::VarifyExpired) {
            emit sig_showTip_changed(tr("验证码过期"), false);
            return;
        }
        else if(err == ErrorCodes::VarifyCodeError) {
            emit sig_showTip_changed(tr("验证码错误"), false);
            return;
        }
        else if(err != ErrorCodes::SUCCESS) {
            emit sig_showTip_changed(tr("参数错误"), false);
            return;
        }
        QString email = jsonObj["email"].toString();
        emit sig_showTip_changed(tr("用户注册成功"), true);
        qDebug() << "User's uuid is: " << jsonObj["uuid"].toInteger() << Qt::endl;
        qDebug() << "User Registed Successed, Email is email" << email << Qt::endl;
        emit sig_reg_mod_success();
    });

    // 注册重置密码的回包逻辑
    _handlers.insert(ReqId::ID_RESET_PWD, [this](const QJsonObject& jsonObj) {
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS) {
            sig_showTip_changed(tr("参数错误"), false);
            return;
        }
        QString email = jsonObj["email"].toString();
        QString uid = jsonObj["uid"].toString();
        sig_showTip_changed(tr("重置成功，点击返回登录"), true);
        qDebug() << "重置密码的用户邮箱是：" << email << " uid是：" << uid << Qt::endl;
    });

    // 注册登录的回包逻辑
    _handlers.insert(ReqId::ID_LOGIN_USER, [this](const QJsonObject& jsonObj) {
        int error = jsonObj["error"].toInt();
        if(error == ErrorCodes::LoginError) {
            emit sig_loginShowTip_changed(tr("用户名或密码错误"), false);
            return;
        }
        if(error != ErrorCodes::SUCCESS) {
            emit sig_loginShowTip_changed(tr("参数错误"), false);
            return;
        }
        // 发送信号通知TcpMgr发送长连接
        ServerInfo serverInfo;
        serverInfo.uid = jsonObj["uid"].toInteger();
        serverInfo.host = jsonObj["host"].toString();
        serverInfo.port = jsonObj["port"].toString();
        serverInfo.token = jsonObj["token"].toString();

        _tcpUid = serverInfo.uid;
        _tcpToken = serverInfo.token;

        qDebug() << "当前登录的回包逻辑，准备发起长连接" << Qt::endl;
        emit sig_connect_tcp(serverInfo);
    });
}

// 发送post请求功能
void HttpMgr::postHttpReq(QUrl url, QJsonObject json, ReqId reqId, Modules mod)
{
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(data.length()));
    // 将data 字节数组的内容发送到request 指定的目的地。（post函数）
    QNetworkReply* reply = _manager.post(request, data);
    qDebug() << url << " 当前Http请求为" << QString("%1").arg(reqId);
    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        QString res = "";
        ErrorCodes code = ErrorCodes::SUCCESS;

        // 处理错误情况
        if(reply->error() != QNetworkReply::NoError) { // 说明有错误
            code = ErrorCodes::ERR_NETWORK;
            qDebug() << "Qt的post请求错误信息：" << reply->errorString();
        }
        else
            res = QString(reply->readAll());

        // 发送信号通知完成
        emit sig_http_finish(reqId, res, code, mod);
        reply->deleteLater();
        return;
    });
}

// 处理http响应请求，根据post请求信号的槽函数传来的模块信息来转发给特定槽函数
void HttpMgr::slot_http_finish(ReqId id, QString res, ErrorCodes err, Modules mod)
{
    if(err == ErrorCodes::ERR_NETWORK) {
        emit sig_showTip_changed(tr("网络请求错误"), false);
        return;
    }

    // 发送信号给注册模块
    switch (mod) {
        case Modules::REGISTERMOD: emit sig_reg_mod_finish(id, res); return;
        case Modules::RESETMOD: emit sig_reset_mod_finish(id, res); return;
        case Modules::LOGINMOD: emit sig_login_request_mod_finish(id, res); return;
        default: return;
    }
}

// 处理注册信息的post响应
void HttpMgr::slot_reg_finish(ReqId id, QString res)
{
    // 解析JSON字符串, res 转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if(jsonDoc.isNull()) {
        qDebug() << "json为空" << Qt::endl;
        emit sig_showTip_changed(tr("json解析失败"), false);
        return;
    }

    if(!jsonDoc.isObject()) {
        qDebug() << "json无对象" << Qt::endl;
        emit sig_showTip_changed(tr("json解析失败"), false);
        return;
    }
    if(_handlers.contains(id))
        _handlers[id](jsonDoc.object());
    else
        qDebug() << "没有该处理函数，非法id" << Qt::endl;
}

// 处理重置密码的post响应
void HttpMgr::slot_reset_finish(ReqId id, QString res)
{
    // 解析JSON字符串, res 转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if(jsonDoc.isNull()) {
        qDebug() << "json为空" << Qt::endl;
        emit sig_showTip_changed(tr("json解析失败"), false);
        return;
    }

    if(!jsonDoc.isObject()) {
        qDebug() << "json无对象" << Qt::endl;
        emit sig_showTip_changed(tr("json解析失败"), false);
        return;
    }
    if(_handlers.contains(id))
        _handlers[id](jsonDoc.object());
    else
        qDebug() << "没有该处理函数，非法id" << Qt::endl;
}

// 处理登录的post响应
void HttpMgr::slot_login_request_finish(ReqId id, QString res) {
    // 解析JSON字符串, res 转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if(jsonDoc.isNull()) {
        qDebug() << "json为空" << Qt::endl;
        emit sig_showTip_changed(tr("json解析失败"), false);
        return;
    }

    if(!jsonDoc.isObject()) {
        qDebug() << "json无对象" << Qt::endl;
        emit sig_showTip_changed(tr("json解析失败"), false);
        return;
    }

    if(_handlers.contains(id))
        _handlers[id](jsonDoc.object());
    else
        qDebug() << "没有该处理函数，非法id" << Qt::endl;
}

// 处理客户端Tcp连接服务器成功的响应
void HttpMgr::slot_tcpCon_success(bool successful)
{
    if(successful) { // (此时QTcpSocket连接已经建立)
        // _tcpToken 不为空时，说明HTTP 验证后的首次登录(首次缓存)
        if(!_tcpToken.isEmpty()) {
            emit sig_loginShowTip_changed(tr("聊天服务连接成功，正在登录..."), true);
            qDebug() << "聊天服务连接成功，正在登录..." << Qt::endl;
            QJsonObject jsonObj;
            jsonObj["uid"] = QString::number(_tcpUid);
            jsonObj["token"] = _tcpToken;
            QJsonDocument doc(jsonObj);
            QString jsonStr = doc.toJson();
            TcpMgr::getInstance().sendData(ReqId::ID_CHAT_LOGIN, jsonStr.toUtf8());
            _tcpToken.clear();
            _tcpUid = 0;
        }
    }
    else {
        emit sig_loginShowTip_changed(tr("网络异常"), false);
        emit sig_loginBtn_enabled(true);
    }
}

// 处理登录失败的槽处理
void HttpMgr::slot_login_failed(int err)
{
    QString result = QString("登录失败, err is %1").arg(err);
    emit sig_loginShowTip_changed(result, false);
    emit sig_loginBtn_enabled(true);
}

// 处理登录成功的槽处理
void HttpMgr::slot_login_success()
{
    emit sig_loginBtn_enabled(true);
}

/// 下面是一些特殊按钮的交互信号
// 处理“获取验证码”按钮的点击信号
void HttpMgr::slot_send_varifyCode(QString email)
{
    QJsonObject json_obj;
    json_obj["email"] = email;
    postHttpReq(QUrl(gate_url_prefix + "/get_varifyCode"), json_obj, ReqId::ID_GET_VARIFY_CODE, Modules::REGISTERMOD);
}

// 处理“注册界面的确认”按钮的点击信号
void HttpMgr::slot_confirmBtn_clicked(QString user, QString email, QString passwd, QString confirm, QString varifyCode)
{
    // 创建JSON对象，发送http请求注册用户
    QJsonObject jsonObj;
    jsonObj["user"] = user;
    jsonObj["email"] = email;
    jsonObj["passwd"] = passwd;
    jsonObj["confirm"] = confirm;
    jsonObj["varifyCode"] = varifyCode;
    postHttpReq(QUrl(gate_url_prefix + "/user_register"), jsonObj, ReqId::ID_REG_USER, Modules::REGISTERMOD);
}

// 处理“重置密码”按钮的点击信号
void HttpMgr::slot_reset_confirm_clicked(QString user, QString email, QString newPassword, QString newConfirm, QString varifyCode)
{
    // 创建JSON对象，发送http请求更改用户密码
    QJsonObject jsonObj;
    jsonObj["user"] = user;
    jsonObj["email"] = email;
    jsonObj["passwd"] = newPassword;
    jsonObj["confirm"] = newConfirm;
    jsonObj["varifyCode"] = varifyCode;
    postHttpReq(QUrl(gate_url_prefix + "/reset_pwd"), jsonObj, ReqId::ID_RESET_PWD, Modules::RESETMOD);
}

// 处理“登录”按钮点击信号
void HttpMgr::slot_loginBtn_clicked(QString user, QString password)
{
    qDebug() << "loginBtn clicked" << Qt::endl;
    // 判断用户名与密码的合法性交给QML, 这里都是合法的信息
    QJsonObject jsonObj;
    jsonObj["user"] = user;
    jsonObj["password"] = password;
    postHttpReq(QUrl(gate_url_prefix + "/user_login"), jsonObj, ReqId::ID_LOGIN_USER, Modules::LOGINMOD);
}
