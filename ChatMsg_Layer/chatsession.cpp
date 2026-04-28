#include "chatsession.h"
#include "UserData_Layer/usermgr.h"
#include "Net_Layer/tcpmgr.h"
#include <QDateTime>
ChatSession::ChatSession(QString peerUid, int sessionType, ConversationsModel *convModel, QObject *parent):
    QObject(parent),
    _peerUid(peerUid),
    _sessionType(sessionType),
    _convModel(convModel)
{
    _fallbackTimer = new QTimer{this};
    initFallbackTimer();
}

ChatSession::~ChatSession()
{
    destroyMsgModel();
}

bool ChatSession::hasMsgModel() const
{
    return _chatMsgModel != nullptr;
}

void ChatSession::createMsgModel()
{
    if(!_chatMsgModel)
    {
        _chatMsgModel = new ChatMsgModel(this);
        _chatMsgModel->init(_peerUid, _sessionType);
    }
}

void ChatSession::destroyMsgModel()
{
    if(_chatMsgModel) {
        _chatMsgModel->clear();
        _chatMsgModel->deleteLater();
        _chatMsgModel = nullptr;
    }
}

ChatMsgModel *ChatSession::getMsgModel() const
{
    return _chatMsgModel;
}

ConversationInfo ChatSession::getConvInfo() const
{
    ConversationInfo info;
    if(!_convModel->getConversationInfo(_peerUid, _sessionType, info)) {
        // 如果是全新会话，初始化基础数据
        info.peerUid = _peerUid;
        info.lastSeqId = 1;
        info.nextSendSeq = 1;
        info.expectedRecvSeq = 1;
        info.isPinned = false;
        info.isMuted = false;
    }
    return info;
}

void ChatSession::updateConvInfo(const ConversationInfo &info)
{
    _convModel->slot_upsertConversation(info);
}

/// 处理发送逻辑
std::tuple<MsgInfo, ConversationInfo> ChatSession::handleSendMsg(const QString &msgData)
{
    ConversationInfo convInfo = getConvInfo();
    uint64_t seqId = convInfo.nextSendSeq++;

    MsgInfo msgInfo;
    msgInfo.clientMsgId = getClientMsgId();
    msgInfo.seqId = seqId;
    msgInfo.createTime = QDateTime::currentMSecsSinceEpoch();
    msgInfo.peerUid = _peerUid;
    msgInfo.sessionType = _sessionType;
    msgInfo.senderUid = UserMgr::getInstance().uid();
    msgInfo.msgData = msgData;
    msgInfo.msgType = 1;
    msgInfo.isSelf = true;
    msgInfo.sendStatus = 0;
    msgInfo.isRead = 0;

    // 若有 UI 模型，则局部刷新 UI
    if (_chatMsgModel) _chatMsgModel->appendMsg(msgInfo);

    // 同步到 ConversationsModel
    convInfo.lastMsgContent = msgData;
    convInfo.lastMsgType = 1;
    convInfo.lastMsgSenderUid = msgInfo.senderUid;
    convInfo.updateTime = msgInfo.createTime;
    updateConvInfo(convInfo);
    return {msgInfo, convInfo};
}

/// 处理接收逻辑
std::tuple<QList<MsgInfo>, ConversationInfo> ChatSession::handleReceiveMsg(const MsgInfo &msgInfo, bool isActive)
{
    ConversationInfo convInfo = getConvInfo();
    uint64_t peerSeqId = msgInfo.seqId;

    if(peerSeqId < convInfo.expectedRecvSeq) {
        qDebug() << "[ChatSession] 丢弃历史重复包 Seq:" << peerSeqId;
        return {};
    }
    if(peerSeqId > convInfo.expectedRecvSeq) {
        qDebug() << "[ChatSession] 乱序包暂存 Seq:" << peerSeqId;
        _outOfOrderBuffer.emplace(msgInfo);
        _oooCount++; // 乱序计数累加
        if(_oooCount >= 3) {
            qDebug() << "触发快速重传，请求区间: [" << convInfo.expectedRecvSeq << "," << peerSeqId - 1 << "]";
            triggerGapSync(convInfo.expectedRecvSeq, peerSeqId - 1);
            _oooCount = 0; // 重置计数，避免重复触发
        }
        return {};
    }

    _oooCount = 0;
    _lastAdvanceTime = QDateTime::currentMSecsSinceEpoch();
    QList<MsgInfo> msgsToAdd;
    msgsToAdd.append(msgInfo);
    convInfo.expectedRecvSeq++;

    // 释放连带缓冲包
    while(!_outOfOrderBuffer.empty()) {
        if(_outOfOrderBuffer.top().seqId == convInfo.expectedRecvSeq) {
            msgsToAdd.append(_outOfOrderBuffer.top());
            _outOfOrderBuffer.pop();
            convInfo.expectedRecvSeq++;
        }
        else
            break;
    }

    // 取出本批次最后一条消息
    const MsgInfo& backInfo = msgsToAdd.back();
    quint64 newVersion = backInfo.msgId.toULongLong();

    if(newVersion > convInfo.version) {
        convInfo.version = newVersion;
        convInfo.lastMsgContent = backInfo.msgData;
        convInfo.sessionType = backInfo.sessionType;
        convInfo.lastMsgSenderUid = backInfo.senderUid;
        convInfo.lastMsgType = backInfo.msgType;
        convInfo.updateTime = backInfo.createTime;
    }

    int unreadAdd = isActive ? 0 : msgsToAdd.size();
    convInfo.unreadCount += unreadAdd;
    updateConvInfo(convInfo);

    if (_chatMsgModel) _chatMsgModel->appendList(msgsToAdd);

    return {msgsToAdd, convInfo};
}

void ChatSession::initFallbackTimer()
{
    connect(_fallbackTimer, &QTimer::timeout, this, [this]
    {   // 如果缓冲区有消息（说明有空洞），且窗口超过10秒没动了
        if (!_outOfOrderBuffer.empty()) {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            // 允许一次空洞期
            if (now - _lastAdvanceTime > 10000) {
                ConversationInfo convInfo = getConvInfo();
                uint64_t nextAvailable = _outOfOrderBuffer.top().seqId;

                qDebug() << "触发定时器兜底，请求区间: [" << convInfo.expectedRecvSeq << "," << nextAvailable - 1 << "]";
                triggerGapSync(convInfo.expectedRecvSeq, nextAvailable - 1);

                // 强制更新一下时间，防止请求频率过高
                _lastAdvanceTime = now;
            }
        }
    });

    // 5s检测一次
    _fallbackTimer->start(5000);
}

void ChatSession::triggerGapSync(quint64 startSeq, quint64 endSeq)
{
    // 封装请求，告知服务器我缺哪些 Seq
    QJsonObject obj;
    obj["peerUid"] = _peerUid;
    obj["sessionType"] = _sessionType;
    obj["fromSeq"] = QString::number(startSeq);
    obj["toSeq"] = QString::number(endSeq);

    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    TcpMgr::getInstance().sendData(ReqId::ID_MSG_REPAIR_REQ, data);
}
