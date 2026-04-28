#ifndef FRIENDLISTMODEL_H
#define FRIENDLISTMODEL_H

#include <QAbstractListModel>
#include "global.h"

class FriendListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
public:
    enum FriendRoles {
        UidRole = Qt::UserRole + 1,
        UsernameRole,
        EmailRole,
        RemarkRole,
        AvatarUrlRole,
        StatusRole
    };

    explicit FriendListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex{}) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    void init();

    void clear();

private:
    //  添加单个好友（触发局部动态插入动画）
    void internalAddFriend(const QString& uid);

    //  删除单个好友（触发局部动态移除动画）
    void internalRemoveFriend(const QString& uid);

    // 只保留UID列表，指向UserMgr的users表
    QList<QString> m_friendUids;

    // Key: TargetUid, Value: index
    QHash<QString, int> m_indexMap;

    bool m_isResetting = false; // 重置护盾标志位
};

#endif // FRIENDLISTMODEL_H
