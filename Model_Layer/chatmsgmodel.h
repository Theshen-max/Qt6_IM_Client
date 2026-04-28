#ifndef CHATMSGMODEL_H
#define CHATMSGMODEL_H

#include <QAbstractListModel>
#include "global.h"

class ChatMsgModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
public:
    enum ChatRoles {
        MsgDataRole = Qt::UserRole + 1,
        IsSelfRole,
        SendStatusRole
    };

    explicit ChatMsgModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    void appendMsg(const MsgInfo& msgInfo);

    void appendList(const QList<MsgInfo>& msgInfoList);

    void prependList(const QList<MsgInfo>& msgInfoList);

    void updateMsgStatus(const QString& clientMsgId, int status);

    qint64 getFirstMsgCreateTime();

    void init(const QString& peerUid, int sessionType);

    void clear();

    void insertGapMessages(const QList<MsgInfo>& gapMsgs);

    Q_INVOKABLE void fetchMoreHistory() ;

signals:
    void sig_reqLoadHistory(const QString& peerUid, int sessionType, qint64 oldestTime, int limit);

    // 没有更多历史消息的通知信号
    void sig_noMoreData();

private:
    // 聊天列表
    QList<MsgInfo> m_messages;
    // 对端UID
    QString m_peerUid;
    // 会话类型
    int m_sessionType = 1;
    // 防抖锁
    bool m_isFetching = false;
    // 标记本地数据库是否榨干
    bool m_hasNoMore = false;
};

#endif // CHATMSGMODEL_H
