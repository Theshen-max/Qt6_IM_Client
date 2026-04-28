#ifndef FRIENDAPPLYMODEL_H
#define FRIENDAPPLYMODEL_H

#include <QAbstractListModel>
#include "global.h"

class FriendApplyModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int unreadCount READ getUnreadCount NOTIFY unreadCountChanged)

public:
    enum ApplyRoles {
        UidRole = Qt::UserRole + 1,
        IsSenderRole, // 关键：区分是我发出的(true)还是别人发给我的(false)
        UsernameRole,
        UseremailRole,
        AvatarRole,
        GreetingRole,
        StatusRole,
        ApplyTimeRole
    };

    enum ApplyAction {
        ActionSend,    // 客户端主动发起：添加好友、同意/拒绝请求 (需要动态置顶)
        ActionReceive  // 服务端被动推送：SYNC拉取、在线NOTIFY (静默更新，加红点)
    };

    explicit FriendApplyModel(QObject *parent = nullptr);

    // QAbstractListModel 必须重写的接口
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    // 初始化接口
    void init();

    // 清理接口
    void clear();

    // 业务接口
    void loadApplies(const QList<FriendApplyInfo>& applies); // 登录/启动时全量加载

    void upsertApply(const FriendApplyInfo& apply, ApplyAction action); // 增量插入或更新

    std::shared_ptr<FriendApplyInfo> getFriendApplyInfo(const QString& peerUid);

    int getUnreadCount() const;

    // 清空未读数 (当用户点击打开 "新的朋友" 面板时调用)
    Q_INVOKABLE void clearUnreadCount();

signals:
    void unreadCountChanged();

private:
    // Key: TargetUid, Value: index
    QHash<QString, int> m_indexMap;

    QList<FriendApplyInfo> m_applies;

    int m_unreadCount = 0;
};

#endif // FRIENDAPPLYMODEL_H
