#include "searchmodel.h"

SearchModel::SearchModel(QObject *parent) :
    QAbstractListModel(parent)
{
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
}

void SearchModel::setSourceData(QHash<QString, std::shared_ptr<UserInfo>> users)
{
    m_sourceUsers = users;
}

int SearchModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_searchResults.size();
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() >= m_searchResults.count())
        return {};

    // 取出当前行对应的数据
    const auto& user = m_searchResults.at(index.row());
    if(!user) return {};

    switch (role) {
    case UidRole:
        return user->_uid;
    case UsernameRole:
        return user->_username;
    case EmailRole:
        return user->_email;
    default:
        return {};
    }
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[UidRole] = "uid";
    roles[UsernameRole] = "username";
    roles[EmailRole] = "email";
    return roles;
}

std::shared_ptr<UserInfo> SearchModel::getUserInfo(const QString &uid)
{
    for(std::shared_ptr<UserInfo> userInfo: m_searchResults) {
        if(userInfo->_uid == uid)
            return userInfo;
    }
    return nullptr;
}

void SearchModel::search(QString text)
{
    beginResetModel();
    m_searchResults.clear();
    for(auto it = m_sourceUsers.constBegin(); it != m_sourceUsers.constEnd(); ++it)
    {
        const std::shared_ptr<UserInfo> userInfo = it.value();
        if(!userInfo) continue;

        if(userInfo->_username.startsWith(text, Qt::CaseSensitive) || userInfo->_email.startsWith(text, Qt::CaseSensitive)) {
            m_searchResults.append(userInfo);
        }
    }
    endResetModel();
}

void SearchModel::clearSearch()
{
    beginResetModel();
    m_searchResults.clear();
    endResetModel();
}

void SearchModel::setSearchResults(const QList<std::shared_ptr<UserInfo> > &list)
{
    // 通知 QML 视图即将重置
    beginResetModel();
    // 替换为网络查回来的新数据
    m_searchResults = list;
    // 通知 QML 视图重置完毕，立刻渲染最新列表！
    endResetModel();
}

void SearchModel::clear()
{
    beginResetModel();
    m_searchResults.clear();
    m_sourceUsers.clear();
    endResetModel();
}
