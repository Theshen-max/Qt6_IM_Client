#include "friendlistmodel.h"
#include "UserData_Layer/usermgr.h"

FriendListModel::FriendListModel(QObject *parent): QAbstractListModel(parent)
{
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
}

// 初始化配置
void FriendListModel::init()
{
    connect(&UserMgr::getInstance(), &UserMgr::sig_userInfo_changed, this, [this](const QString& uid) {
        auto user = UserMgr::getInstance().getUserInfo(uid);
        if (!user) return;

        bool existsInList = m_indexMap.contains(uid);

        if (existsInList) {
            // 原本在列表中，还是好友属性，说明其他属性值修改
            if (user->_isFriend) {
                int row = m_indexMap[uid];
                emit dataChanged(index(row), index(row));
            }
            // 原本在列表中，但是不是好友属性，说明被移除
            else
                internalRemoveFriend(uid); // 自动触发 QML 列表项消失动画！
        }
        else {
            // 原本不再列表中，但其是好友属性，说明新加入
            if (user->_isFriend) {
                internalAddFriend(uid);
            }
        }
    });

    connect(&UserMgr::getInstance(), &UserMgr::sig_allUserCache_loaded, this, [this]() {
        // 开启重置护盾
        m_isResetting = true;
        beginResetModel();
        m_friendUids.clear();
        m_indexMap.clear();

        auto allUsers = UserMgr::getInstance().getAllUsers();
        for (auto it = allUsers.begin(); it != allUsers.end(); ++it) {
            if (it.value()->_isFriend) {
                m_friendUids.append(it.key());
                m_indexMap[it.key()] = m_friendUids.size() - 1;
            }
        }

        endResetModel();
        m_isResetting = false;
        qDebug() << "[FriendListModel] 侦测到中央缓存就绪，已自动提取并重置好友列表！";
    });
}

int FriendListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_friendUids.size());
}

QVariant FriendListModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() < 0 || index.row() > m_friendUids.size())
        return {};

    QString uid = m_friendUids.at(index.row());

    auto user = UserMgr::getInstance().getUserInfo(uid);
    if(!user) return {};

    switch(role) {
        case FriendRoles::UidRole: return user->_uid;
        case FriendRoles::UsernameRole: return user->_username;
        case FriendRoles::EmailRole: return user->_email;
        case FriendRoles::RemarkRole: return user->_remark;
        case FriendRoles::AvatarUrlRole: return user->_avatarUrl;
        case FriendRoles::StatusRole: return user->_status;
        default: return {};
    }
}

QHash<int, QByteArray> FriendListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FriendRoles::UidRole] = "uid";
    roles[FriendRoles::UsernameRole] = "username";
    roles[FriendRoles::EmailRole] = "email";
    roles[FriendRoles::RemarkRole] = "remark";
    roles[FriendRoles::AvatarUrlRole] = "avatarUrl";
    roles[FriendRoles::StatusRole] = "status";
    return roles;
}

void FriendListModel::internalAddFriend(const QString &uid)
{
    if(m_indexMap.contains(uid)) return;

    int row = m_friendUids.size();
    beginInsertRows(QModelIndex(), row, row);
    m_friendUids.append(uid);
    endInsertRows();

    m_indexMap[uid] = row;
}

void FriendListModel::internalRemoveFriend(const QString &uid)
{
    if(!m_indexMap.contains(uid)) return;
    
    int row = m_indexMap[uid];
    beginRemoveRows(QModelIndex(), row, row);
    m_friendUids.removeAt(row);
    endRemoveRows();

    // 索引重排
    for(int i = row; i < m_friendUids.size(); ++i) {
        m_indexMap[m_friendUids[i]] = i;
    }

    m_indexMap.remove(uid);
}


void FriendListModel::clear()
{
    beginResetModel();
    m_friendUids.clear();
    m_indexMap.clear();
    endResetModel();
}
