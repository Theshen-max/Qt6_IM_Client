#ifndef TCPMGR_H
#define TCPMGR_H

#include "global.h"
#include "tcpworker.h"
class TcpMgr: public QObject
{
    Q_OBJECT
public:
    TcpMgr(const TcpMgr&) = delete;

    TcpMgr& operator=(const TcpMgr&) = delete;

    ~TcpMgr();

    static TcpMgr& getInstance();

    void connectToServer(const QString& host, quint16 port);

    void sendData(ReqId reqId, const QByteArray& data);

    void disconnectServer();

    void init();

public slots:
    void slot_tcp_connect(ServerInfo info);

signals:
    // ---> 发送给后台 TcpWorker 的信号
    void sig_internal_connect(QString host, quint16 port);

    void sig_internal_send(ReqId reqId, QByteArray dataArray);

    void sig_internal_disconnect();

    // 连接状态相关信号 (可供 UI 或 LogicMgr 监听)
    void sig_connect_success(bool success);

    void sig_reconnect_success(bool success);

    void sig_net_state_changed(NetState state);

    void sig_socket_error(QAbstractSocket::SocketError err);

    void sig_disconnect();

private:
    TcpMgr(QObject* parent = nullptr);

    QThread* _thread{};

    TcpWorker* _tcpWorker{};
};

#endif // TCPMGR_H
