#include "conversationsmodel.h"
#include "UserData_Layer/usermgr.h"
#include "Sqlite_Layer/sqlitemgr.h"

ConversationsModel::ConversationsModel(QObject *parent)
    : QAbstractListModel{parent}
{
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
}

int ConversationsModel::rowCount(const QModelIndex &parent) const
{
    if(parent.isValid()) return 0;
    return static_cast<int>(m_conversationsList.size());
}

QVariant ConversationsModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() < 0 || index.row() > m_conversationsList.size()) return {};

    const ConversationInfo& info = m_conversationsList[index.row()];
    switch(role) {
        case PeerUidRole: return info.peerUid;
        case SessionTypeRole: return info.sessionType;
        case LastMsgRole: return info.lastMsgContent;
        case UnreadCountRole: return info.unreadCount;
        case UpdateTimeRole: return info.updateTime;
        case IsPinnedRole: return info.isPinned;
        case IsMutedRole: return info.isMuted;
        case NameRole: {
            auto userInfo = UserMgr::getInstance().getUserInfo(info.peerUid);
            if (userInfo)
                return userInfo->_remark.isEmpty() ? userInfo->_username : userInfo->_remark;
            return info.peerUid;
        }
        case AvatarRole: {
            auto userInfo = UserMgr::getInstance().getUserInfo(info.peerUid);
            return userInfo ? userInfo->_avatarUrl : "";
        }
        default: return {};
    }
}

QHash<int, QByteArray> ConversationsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[PeerUidRole] = "peerUid";
    roles[SessionTypeRole] = "sessionType";
    roles[NameRole] = "username";
    roles[AvatarRole] = "avatarUrl";
    roles[LastMsgRole] = "lastMsgContent";
    roles[UnreadCountRole] = "unreadCount";
    roles[UpdateTimeRole] = "updateTime";
    roles[IsPinnedRole] = "isPinned";
    roles[IsMutedRole] = "isMuted";
    return roles;
}

bool ConversationsModel::getConversationInfo(const QString &peerUid, int sessionType, ConversationInfo &outInfo) const
{
    QString key = formatSessionKey(peerUid, sessionType);
    if(!m_indexMap.contains(key)) return false;
    outInfo = m_conversationsList[m_indexMap[key]];
    return true;
}

void ConversationsModel::slot_conversation_userInfo_changed(const QString &uid)
{
    if(!m_indexMap.contains(uid)) return;

    int row = m_indexMap[uid];

    QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

void ConversationsModel::slot_resetConversations(const QList<ConversationInfo> &list)
{
    beginResetModel();
    m_conversationsList = list;
    endResetModel();
    m_indexMap.clear();
    rebuildHashIndex(0, list.size());
}

void ConversationsModel::slot_upsertConversation(const ConversationInfo &info)
{
    QString key = formatSessionKey(info.peerUid, info.sessionType);
    // 已存在，更新
    if(m_indexMap.contains(key)) {
        int oldRow = m_indexMap[key];
        int targetRow = calculateInsertRow(info, oldRow);

        if(targetRow != oldRow) {
            int insertRow = (targetRow > oldRow) ? targetRow + 1 : targetRow;
            beginMoveRows(QModelIndex(), oldRow, oldRow, QModelIndex(), insertRow);
            m_conversationsList.move(oldRow, targetRow);
            endMoveRows();

            int startIndex = qMin(targetRow, oldRow);
            int endIndex = qMax(targetRow, oldRow) + 1;
            rebuildHashIndex(startIndex, endIndex);   
        }

        m_conversationsList[targetRow] = info;
        QModelIndex idx = index(targetRow);
        emit dataChanged(idx, idx, {LastMsgRole, UnreadCountRole, UpdateTimeRole, IsPinnedRole, IsMutedRole});
    }
    // 未存在，添加
    else {
        int targetRow = calculateInsertRow(info, -1);
        beginInsertRows(QModelIndex(), targetRow, targetRow);
        m_conversationsList.insert(targetRow, info);
        endInsertRows();
        rebuildHashIndex(targetRow, m_conversationsList.size());
    }
}

void ConversationsModel::slot_removeConversation(const QString &peerUid, int sessionType)
{
    QString key = formatSessionKey(peerUid, sessionType);
    if(!m_indexMap.contains(key)) return;

    int row = m_indexMap[key];
    beginRemoveRows(QModelIndex(), row, row);
    m_conversationsList.remove(row);
    endRemoveRows();
    m_indexMap.remove(key);
    rebuildHashIndex(row, m_conversationsList.size());
}

void ConversationsModel::clearUnread(const QString &peerUid, int sessionType)
{
    QString key = formatSessionKey(peerUid, sessionType);
    if(!m_indexMap.contains(key)) return;
    int row = m_indexMap[key];
    if(m_conversationsList[row].unreadCount > 0) {
        m_conversationsList[row].unreadCount = 0;

        // 触发QML UI局部刷新
        QModelIndex idx = index(row);
        emit dataChanged(idx, idx, {UnreadCountRole});
    }
}

void ConversationsModel::clear()
{
    beginResetModel();
    m_conversationsList.clear();
    m_indexMap.clear();
    endResetModel();
}

void ConversationsModel::init()
{
    connect(&UserMgr::getInstance(), &UserMgr::sig_userInfo_changed, this, &ConversationsModel::slot_conversation_userInfo_changed);
}

void ConversationsModel::rebuildHashIndex(int startIndex, int endIndex)
{
    for (int i = startIndex; i < endIndex; ++i) {
        // 使用复合键重建索引
        QString key = formatSessionKey(m_conversationsList[i].peerUid, m_conversationsList[i].sessionType);
        m_indexMap[key] = i;
    }
}

int ConversationsModel::calculateInsertRow(const ConversationInfo &info, int ignoreRow) const
{
    // 注意这里的排序会参考多个属性值，不能简单返回i，要自己设计返回索引
    // 不采用直接 info.peerUid == current.peerUid的原因是，自身不是常量（删除历史消息后，会话last信息，时间等等都会改变）
    int target = 0;
    for(int i = 0; i < m_conversationsList.size(); ++i) {
        if(i == ignoreRow) continue;

        const ConversationInfo& current = m_conversationsList[i];

        if(info.isPinned && !current.isPinned) return target;

        if(!info.isPinned && current.isPinned) {
            target++;
            continue;
        }

        if(info.updateTime > current.updateTime) return target;

        target++;
    }
    return target;
}

