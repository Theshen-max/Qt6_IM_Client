#ifndef SEARCHMODEL_H
#define SEARCHMODEL_H

#include <QAbstractListModel>
#include "global.h"

class SearchModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
public:

    // 定义 QML 中可以访问的属性名称 (Role)
    enum UserRoles {
        UidRole = Qt::UserRole + 1,
        UsernameRole,
        EmailRole
    };

    explicit SearchModel(QObject *parent = nullptr);

    // 设置底层数据源的引用 (在初始化时传入 TcpMgr 或 UserMgr 中的 _users)
    void setSourceData(QHash<QString, std::shared_ptr<UserInfo>> users);

    // QAbstractListModel 必须重写的三个核心函数
    // 告诉多少行
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    // 告诉每个role对应的数据
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // 告诉每个role映射的字符串
    QHash<int, QByteArray> roleNames() const override;

    std::shared_ptr<UserInfo> getUserInfo(const QString& uid);

    void search(QString text);

    void clearSearch();

    void setSearchResults(const QList<std::shared_ptr<UserInfo>>& list);

    void clear();

private:
    // 指向全局唯一数据源，SearchModel 绝对不管理数据的生命周期
    QHash<QString, std::shared_ptr<UserInfo>> m_sourceUsers;

    // 存放搜索结果的列表（存放智能指针拷贝，拷贝成本极低，且安全）
    QList<std::shared_ptr<UserInfo>> m_searchResults;
};

#endif // SEARCHMODEL_H
