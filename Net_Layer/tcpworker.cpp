#include "tcpworker.h"
#include "Logic_Layer/logicmgr.h"
#include <QtEndian>

static constexpr int BUFFER_CAP = 4096;
TcpWorker::TcpWorker(QObject *parent)
    : QObject{parent}
{
    // 在这个函数里 new Socket，确保它生存在独立的 Network 线程
    _socket = new QSslSocket(this);
    _heartbeatTimer = new QTimer(this);
    _reconnectTimer = new QTimer(this);
}

TcpWorker::~TcpWorker()
{
    slot_disconnect();
}

void TcpWorker::slot_init()
{
    _buffer.reserve(BUFFER_CAP);
    _heartbeatTimer->setInterval(15000);
    _reconnectTimer->setInterval(_currentReconnectDelay);
    _reconnectTimer->setSingleShot(true);

    connect(_socket, &QSslSocket::encrypted, this, [this] {
        qDebug() << "[TcpWorker] Connected to Server encrypted!";

        // 依靠状态机来区分是“首次连接”还是“断线重连”
        bool isReconnecting = (_state == NetState::Reconnecting);

        // 切换到在线状态
        changeState(NetState::Connected);

        // 根据情况发射不同的成功信号
        if (isReconnecting) {
            qDebug() << "[TcpWorker] 识别为断线重连，发射 sig_reconnect_success";
            emit sig_reconnect_success(true);
        }
        else {
            qDebug() << "[TcpWorker] 识别为首次连接，发射 sig_connect_success";
            emit sig_connect_success(true);
        }

        // 重置所有的重连阶梯参数
        _currentReconnectDelay = 2000;
        _reconnectCount = 0;

        _buffer.clear();
        _message_id = 0;
        _message_len = 0;
        _message_hasData = false;

        // 保险关闭，记住reconnectTimer的作用只是隔时间段启动一次连接（singleton）, 不能根据其状态判断当前NetState
        if(_reconnectTimer->isActive()) {
            _reconnectTimer->stop();
        }

        // 启动心跳
        _heartbeatTimer->start();
    });

    connect(_socket, &QSslSocket::disconnected, this, [this]{
        qDebug() << "[TcpWorker] Disconnected from server.";

        // 关闭HeartbeatTimer
        if(_heartbeatTimer->isActive())
            _heartbeatTimer->stop();

        // 判断是否主动断开连接
        if(_state != NetState::Disconnected)
            slot_do_reconnect();
        else
            emit sig_disconnect();
    });

    connect(_socket, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError err) {
        qDebug() << "[TcpWorker] Socket Error:" << _socket->errorString();

        // 关闭HeartbeatTimer
        if(_heartbeatTimer->isActive())
            _heartbeatTimer->stop();

        // 发送Socket错误信号
        emit sig_socket_error(err);

        // 尝试重连
        if(_state != NetState::Disconnected)
            slot_do_reconnect();
    });

    connect(_socket, &QSslSocket::readyRead, this, &TcpWorker::slot_readyRead);

    connect(this, &TcpWorker::sig_data_received, this, &TcpWorker::slot_recv_data);

    /// 应用层心跳机制
    connect(_heartbeatTimer, &QTimer::timeout, this, [this]{
        if(_state != NetState::Connected) return; // 安全防御

        // 打印日志
        qDebug() << "当前是心跳机制";

        // 发送心跳包
        slot_send_data(ID_HEARTBEAT_REQ, "{}");
    });

    /// 应用层重连机制
    connect(_reconnectTimer, &QTimer::timeout, this, [this]{
        if(_state == NetState::Disconnected) return; // 安全防御

        // 打印日志
        qDebug() << "[TcpWorker] Attempting to reconnect to" << _lastHost << ":" << _lastPort;

        // 先把旧连接的垃圾清理掉，再发起新连接
        _socket->abort(); // abort()会终止当前连接并重置套接字，disconnectFromHost()则不会立即关闭当前连接（如果有等待写入的数据）

        // 重连
        _socket->connectToHostEncrypted(_lastHost, _lastPort);
    });
}

void TcpWorker::slot_connect_server(QString host, quint16 port)
{
    // 缓存数据
    _lastHost = host;
    _lastPort = port;

    // 打印日志
    qDebug() << "[TcpWorker] Connecting to" << host << ":" << port;

    // 清理残余状态并启动连接
    _socket->abort();

    // 切换网络状态
    changeState(NetState::Connecting);

    _socket->connectToHostEncrypted(host, port);
}

/// 接收数据
void TcpWorker::slot_readyRead()
{
    _buffer.append(_socket->readAll());
    while(true) {
        // 已经解析完一整个包体，或者第一次解析
        if(!_message_hasData) {
            if(_buffer.size() < sizeof(quint16) * 2) return;

            const unsigned char* dataPtr = reinterpret_cast<const unsigned char*>(_buffer.constData());
            _message_id = qFromLittleEndian<quint16>(dataPtr);
            _message_len = qFromLittleEndian<quint16>(dataPtr + sizeof(quint16));
            _buffer.remove(0, sizeof(quint16) * 2);
        }
        if(_buffer.size() < _message_len) {
            _message_hasData = true;
            return;
        }
        _message_hasData = false;
        QByteArray data = _buffer.sliced(0, _message_len);
        _buffer.remove(0, _message_len);
        emit sig_data_received(static_cast<ReqId>(_message_id), data);
        qDebug() << "收到ReqId: " << _message_id << " 请求";
    }
}

void TcpWorker::slot_recv_data(ReqId reqId, QByteArray dataArray)
{
    LogicMgr::getInstance().dispatchNetDataAsync(reqId, dataArray);
}

void TcpWorker::slot_do_reconnect()
{
    // 幂等判断
    if(_state == NetState::Disconnected || _reconnectTimer->isActive()) return;

    // 计算有无达到最多重连次数(先判断有没有资格重传)
    _reconnectCount++;
    if(_reconnectCount > MAX_RECONNECT_COUNT) {
        qWarning() << "[TcpWorker] 重连达到最大次数 (" << MAX_RECONNECT_COUNT << ")，彻底放弃治疗！";
        emit sig_reconnect_success(false);
        slot_do_reset();
        return;
    }

    // 切换网络状态
    changeState(NetState::Reconnecting);

    // 打印日志并启动重连定时器
    qDebug() << "[TcpWorker] Schedule reconnect in" << _currentReconnectDelay << "ms.";
    _reconnectTimer->start(_currentReconnectDelay);

    // 计算下一次梯度时间
    _currentReconnectDelay *= 2;
    _currentReconnectDelay = qMin(_currentReconnectDelay, MAX_RECONNECT_DELAY);
}

void TcpWorker::slot_do_reset()
{
    qWarning() << "[TcpWorker] 执行强制重置 (Abort)!";
    slot_do_clear();

    if(_socket)
        _socket->abort();

    emit sig_reset();
}

void TcpWorker::slot_do_clear()
{
    // 切换网络状态
    changeState(NetState::Disconnected);

    if (_reconnectTimer && _reconnectTimer->isActive()) {
        _reconnectTimer->stop();
    }
    if (_heartbeatTimer && _heartbeatTimer->isActive()) {
        _heartbeatTimer->stop();
    }

    _currentReconnectDelay = 2000;
    _reconnectCount = 0;

    _buffer.clear();
    _message_id = 0;
    _message_len = 0;
    _message_hasData = false;
}

void TcpWorker::changeState(NetState newState)
{
    if(_state != newState) {
        _state = newState;
        emit sig_net_state_changed(_state);
    }
}

/// 发送数据
void TcpWorker::slot_send_data(ReqId reqId, QByteArray dataArray)
{
    if(_state == NetState::Connected) {
        QByteArray block;
        quint16 id = qToLittleEndian(reqId);
        quint16 len = qToLittleEndian(static_cast<quint16>(dataArray.size()));

        block.reserve(sizeof(quint16) * 2 + len);
        block.append(reinterpret_cast<const char*>(&id), sizeof(id));
        block.append(reinterpret_cast<const char*>(&len), sizeof(len));
        block.append(dataArray);

        _socket->write(block);
    }
    else
        qDebug() << "[TcpWorker] 正在连接中/重连中/断线中";
}

/// 用户主动注销/退出程序
void TcpWorker::slot_disconnect()
{
    qDebug() << "[TcpWorker] 用户主动断开连接...";
    slot_do_clear();

    if (_socket && _socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->disconnectFromHost();
        _socket->waitForDisconnected(1000);
    }
}

void TcpWorker::slot_shutdown()
{
    // 1. 断开 socket
    slot_disconnect();

    // 2. 停 timer
    // 3. 清理资源
    // 4. 发信号通知可以结束线程了
    emit sig_shutdown_finished();
}
