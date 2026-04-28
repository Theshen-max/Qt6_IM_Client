#ifndef CONVERSATIONSMODEL_H
#define CONVERSATIONSMODEL_H

#include <QAbstractListModel>
#include "global.h"

class ConversationsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
public:
    enum ConversationRoles {
        PeerUidRole = Qt::UserRole + 1,
        SessionTypeRole,
        NameRole,
        AvatarRole,
        LastMsgRole,
        UnreadCountRole,
        UpdateTimeRole,
        IsPinnedRole,
        IsMutedRole
    };

    explicit ConversationsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    bool getConversationInfo(const QString& peerUid, int sessionType, ConversationInfo& outInfo) const;

    void clearUnread(const QString& peerUid, int sessionType);

    void clear();

    void init();

public slots:
    void slot_conversation_userInfo_changed(const QString& uid);

    void slot_resetConversations(const QList<ConversationInfo>& list);

    void slot_upsertConversation(const ConversationInfo& info);

    void slot_removeConversation(const QString& peerUid, int sessionType);

private:
    void rebuildHashIndex(int startIndex, int endIndex);

    int calculateInsertRow(const ConversationInfo& info, int ignoreRow) const;

    QString formatSessionKey(const QString& uid, int type) const {
        return uid + "_" + QString::number(type);
    }

private:
    // 内部维护一个有序的列表，这是 QML ListView 真正的直接数据源
    QList<ConversationInfo> m_conversationsList;

    // Key: FormatSessionKey, Value: index
    QHash<QString, int> m_indexMap;

signals:
};

#endif // CONVERSATIONSMODEL_H
