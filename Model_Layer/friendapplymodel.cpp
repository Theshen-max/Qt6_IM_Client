#include "friendapplymodel.h"
#include "UserData_Layer/usermgr.h"

FriendApplyModel::FriendApplyModel(QObject *parent)
    : QAbstractListModel{parent}
{
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
}

int FriendApplyModel::rowCount(const QModelIndex &parent) const
{
    if(parent.isValid())
        return 0;

    return static_cast<int>(m_applies.size());
}

QVariant FriendApplyModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() < 0 || index.row() >= m_applies.size()) return {};

    const FriendApplyInfo& applyInfo = m_applies[index.row()];

    QString myUid = UserMgr::getInstance().uid();
    QString peerUid = (applyInfo.fromUid == myUid) ? applyInfo.toUid : applyInfo.fromUid;
    auto peerInfo = UserMgr::getInstance().getUserInfo(peerUid);

    switch (role) {
        case UidRole: return peerUid;
        case IsSenderRole: return applyInfo.fromUid == myUid;
        case UsernameRole: return peerInfo ? peerInfo->_username : "未知用户";
        case UseremailRole: return peerInfo ? peerInfo->_email : "未知邮箱";
        case AvatarRole: return peerInfo ? peerInfo->_avatarUrl : "";
        case GreetingRole: return applyInfo.greeting;
        case StatusRole: return applyInfo.status;
        case ApplyTimeRole: return applyInfo.applyTime;
        default: return {};
    }
}

QHash<int, QByteArray> FriendApplyModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[UidRole] = "uid";
    roles[IsSenderRole] = "isSender"; // true=我发出的, false=别人发给我的
    roles[UsernameRole] = "username";
    roles[UseremailRole] = "email";
    roles[AvatarRole] = "avatarUrl";
    roles[GreetingRole] = "greeting";
    roles[StatusRole] = "status";     // 0:待处理, 1:已同意, 2:已拒绝
    roles[ApplyTimeRole] = "applyTime";
    return roles;
}

void FriendApplyModel::init()
{
    // 绑定用户基本信息
    connect(&UserMgr::getInstance(), &UserMgr::sig_userInfo_changed, this, [this](const QString& updatedUid) {
        if (m_indexMap.contains(updatedUid)) {
            int row = m_indexMap[updatedUid];
            QModelIndex idx = index(row);
            emit dataChanged(idx, idx, {UsernameRole, UseremailRole, AvatarRole});
        }
    });
}

void FriendApplyModel::clear()
{
    beginResetModel();
    m_applies.clear();
    m_indexMap.clear();
    endResetModel();
    m_unreadCount = 0;
    emit unreadCountChanged();
}

void FriendApplyModel::loadApplies(const QList<FriendApplyInfo> &applies)
{
    // 触发批量重置视图
    beginResetModel();

    m_applies = applies;
    m_indexMap.clear();
    m_unreadCount = 0;

    QString myUid = UserMgr::getInstance().uid();

    for (int i = 0; i < m_applies.size(); ++i) {
        const FriendApplyInfo& apply = m_applies[i];

        // 计算对方的 UID 作为哈希映射的 Key
        QString peerUid = (apply.fromUid == myUid) ? apply.toUid : apply.fromUid;
        m_indexMap[peerUid] = i;

        // 如果是别人发给我的，且状态为待处理 (0)，则算作未读红点
        if (apply.toUid == myUid && apply.status == 0) {
            m_unreadCount++;
        }
    }

    endResetModel();

    // 通知 QML 更新总红点数
    emit unreadCountChanged();
}

void FriendApplyModel::upsertApply(const FriendApplyInfo &apply, ApplyAction action)
{
    QString myUid = UserMgr::getInstance().uid();
    QString peerUid = (apply.fromUid == myUid) ? apply.toUid : apply.fromUid;

    bool isIncomingPending = (apply.toUid == myUid && apply.status == 0);

    if (m_indexMap.contains(peerUid)) {
        // 记录已存在
        int row = m_indexMap[peerUid];

        // 更新数据
        m_applies[row] = apply;

        if (action == ActionSend) {
            if (row != 0) {
                beginMoveRows(QModelIndex(), row, row, QModelIndex(), 0);
                m_applies.move(row, 0);
                endMoveRows();

                for (int i = 0; i <= row; ++i) {
                    QString pUid = (m_applies[i].fromUid == myUid) ? m_applies[i].toUid : m_applies[i].fromUid;
                    m_indexMap[pUid] = i;
                }
            }
            emit dataChanged(index(0), index(0));
        }
        else
            emit dataChanged(index(row), index(row));
    }
    else {
        // 记录不存在，直接插入到最顶部
        beginInsertRows(QModelIndex(), 0, 0);
        m_applies.prepend(apply);

        // 现有的所有行号 + 1
        for (auto it = m_indexMap.begin(); it != m_indexMap.end(); ++it) {
            it.value()++;
        }

        // 记录新插入的数据索引
        m_indexMap[peerUid] = 0;
        endInsertRows();

        // 如果是收到新的请求，增加红点
        if (action == ActionReceive && isIncomingPending) {
            m_unreadCount++;
            emit unreadCountChanged();
        }
    }
}

std::shared_ptr<FriendApplyInfo> FriendApplyModel::getFriendApplyInfo(const QString &peerUid)
{
    if(!m_indexMap.contains(peerUid))
        return {};

    return std::make_shared<FriendApplyInfo>(m_applies[m_indexMap[peerUid]]);
}

int FriendApplyModel::getUnreadCount() const
{
    return m_unreadCount;
}

void FriendApplyModel::clearUnreadCount()
{
    if (m_unreadCount > 0) {
        m_unreadCount = 0;
        emit unreadCountChanged();
    }
}
