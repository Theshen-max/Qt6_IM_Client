#include "tcpmgr.h"
#include <QtEndian>
#include <QJsonDocument>
#include <QJsonArray>

TcpMgr::TcpMgr(QObject* parent) :
    QObject(parent)
{
    _thread = new QThread(this);
    _tcpWorker = new TcpWorker();
    _tcpWorker->moveToThread(_thread);
}

TcpMgr::~TcpMgr()
{
    if (_thread && _thread->isRunning()) {

        // 1. worker 在线程内收尾
        QMetaObject::invokeMethod(_tcpWorker, "slot_shutdown", Qt::BlockingQueuedConnection);

        // 2. 再结束线程
        _thread->quit();
        _thread->wait(1000);
    }
}

TcpMgr &TcpMgr::getInstance()
{
    static TcpMgr instance;
    return instance;
}

void TcpMgr::connectToServer(const QString &host, quint16 port)
{
    emit sig_internal_connect(host, port);
}

void TcpMgr::sendData(ReqId reqId, const QByteArray &data)
{
    emit sig_internal_send(reqId, data);
}

void TcpMgr::disconnectServer()
{
    emit sig_internal_disconnect();
}

void TcpMgr::init()
{
    /// TcpMgr--->TcpWorker
    connect(_thread, &QThread::started, _tcpWorker, &TcpWorker::slot_init);
    connect(_thread, &QThread::finished, _tcpWorker, &QObject::deleteLater);
    connect(this, &TcpMgr::sig_internal_connect, _tcpWorker, &TcpWorker::slot_connect_server);
    connect(this, &TcpMgr::sig_internal_send, _tcpWorker, &TcpWorker::slot_send_data);
    connect(this, &TcpMgr::sig_internal_disconnect, _tcpWorker, &TcpWorker::slot_disconnect);

    /// TcpWorker--->TcpMgr
    connect(_tcpWorker, &TcpWorker::sig_connect_success, this, &TcpMgr::sig_connect_success);
    connect(_tcpWorker, &TcpWorker::sig_socket_error, this, &TcpMgr::sig_socket_error);
    connect(_tcpWorker, &TcpWorker::sig_disconnect, this, &TcpMgr::sig_disconnect);
    connect(_tcpWorker, &TcpWorker::sig_reconnect_success, this, &TcpMgr::sig_reconnect_success);
    connect(_tcpWorker, &TcpWorker::sig_net_state_changed, this, &TcpMgr::sig_net_state_changed);

    _thread->start();
}

void TcpMgr::slot_tcp_connect(ServerInfo info)
{
    connectToServer(info.host, info.port.toUShort());
}





