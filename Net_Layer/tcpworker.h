#ifndef TCPWORKER_H
#define TCPWORKER_H

#include <QObject>
#include "global.h"

enum class NetState {
    Disconnected = 0,   // 用户主动断开，或尚未连接
    Connecting,     // 正在尝试初始连接
    Connected,      // 已连接且加密就绪
    Reconnecting    // 断线重连中
};

class TcpWorker : public QObject
{
    Q_OBJECT
public:
    explicit TcpWorker(QObject *parent = nullptr);

    ~TcpWorker();

public slots:
    void slot_init(); // 必须在这里 new Socket，绑定线程

    void slot_connect_server(QString host, quint16 port);

    void slot_send_data(ReqId reqId, QByteArray dataArray); // 接收组装好的字节流发送

    void slot_disconnect();

    void slot_shutdown();

private slots:
    void slot_readyRead();

    void slot_recv_data(ReqId reqId, QByteArray dataArray);

    // 重连触发槽
    void slot_do_reconnect();

    void slot_do_reset();

    void slot_do_clear();

signals:
    void sig_data_received(ReqId reqId, QByteArray data);

    void sig_socket_error(QAbstractSocket::SocketError err);

    void sig_connect_success(bool success);

    void sig_reconnect_success(bool success);

    void sig_disconnect();

    void sig_reset();

    void sig_shutdown_finished();

    // 向外部暴露状态变化
    void sig_net_state_changed(NetState state);

private:
    /// 底层通信socket
    QSslSocket* _socket = nullptr;

    /// 心跳与重连定时器
    QTimer* _heartbeatTimer{};
    QTimer* _reconnectTimer{};

    /// 梯度重连延迟参数
    int _reconnectCount = 0;
    int _currentReconnectDelay = 2000;
    constexpr static int MAX_RECONNECT_DELAY = 30000;
    constexpr static int MAX_RECONNECT_COUNT = 5;

    /// 网络状态与changed辅助函数
    NetState _state = NetState::Disconnected;
    void changeState(NetState newState);

    /// 目标服务器IP + Port
    QString _lastHost;
    quint16 _lastPort = 0;

    /// 处理TCP粘包数据
    QByteArray _buffer;
    quint16 _message_id = 0;
    quint16 _message_len = 0;
    bool _message_hasData = false;
};

#endif // TCPWORKER_H
