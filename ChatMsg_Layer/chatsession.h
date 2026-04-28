#ifndef CHATSESSION_H
#define CHATSESSION_H

#include <QObject>
#include "Model_Layer/chatmsgmodel.h"
#include "Model_Layer/conversationsmodel.h"
#include <queue>

struct Compare {
    bool operator()(const MsgInfo& a, const MsgInfo& b) {
        return a.seqId > b.seqId;
    }
};

class ChatSession : public QObject
{
    Q_OBJECT
public:
    ChatSession(QString peerUid, int sessionType, ConversationsModel* convModel, QObject* parent = nullptr);

    ~ChatSession();

    // Model 生命周期管理 (由 LRU 算法调用)
    bool hasMsgModel() const;

    void createMsgModel();

    void destroyMsgModel();

    ChatMsgModel* getMsgModel() const;

    // 数据同步接口 (通过它来操作 ConversationsModel)
    ConversationInfo getConvInfo() const;

    void updateConvInfo(const ConversationInfo& info);

    // 核心业务：发送与接收黑盒
    std::tuple<MsgInfo, ConversationInfo> handleSendMsg(const QString& msgData);

    std::tuple<QList<MsgInfo>, ConversationInfo> handleReceiveMsg(const MsgInfo& msgInfo, bool isActive);

private:
    void initFallbackTimer();

    // 触发空洞拉取请求
    void triggerGapSync(quint64 startSeq, quint64 endSeq);

private:
    QString _peerUid;

    int _sessionType;

    ConversationsModel* _convModel;

    ChatMsgModel* _chatMsgModel = nullptr; // 显存大户，生命周期受控

    std::priority_queue<MsgInfo, std::vector<MsgInfo>, Compare> _outOfOrderBuffer; // 乱序缓冲区

    /// 自愈机制
    int _oooCount{};

    qint64 _lastAdvanceTime{};

    QTimer* _fallbackTimer{};
};

#endif // CHATSESSION_H
