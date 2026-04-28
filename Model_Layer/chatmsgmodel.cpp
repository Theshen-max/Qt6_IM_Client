#include "chatmsgmodel.h"
#include "Sqlite_Layer/sqlitemgr.h"

ChatMsgModel::ChatMsgModel(QObject *parent):
    QAbstractListModel(parent)
{
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);

    /// ChatMsgModel ---> SqliteMgr
    connect(this, &ChatMsgModel::sig_reqLoadHistory, &SqliteMgr::getInstance(), &SqliteMgr::sig_getChatHistory);

    /// SqliteMgr ---> ChatMsgModel
    connect(&SqliteMgr::getInstance(), &SqliteMgr::sig_getChatHistoryResult, this, [this](QString peerUid, int sessionType, QList<MsgInfo> historyList){
        if(peerUid == m_peerUid && sessionType == m_sessionType) {
            if (historyList.size() < 20) {
                emit sig_noMoreData(); // 触发信号，通知 QML
            }

            // 倒灌数据
            if (!historyList.isEmpty()) {
                prependList(historyList);
            }

            m_isFetching = false;
        }
    });
}

int ChatMsgModel::rowCount(const QModelIndex &parent) const
{
    if(parent.isValid()) return 0;
    return m_messages.size();
}

QVariant ChatMsgModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() < 0 || index.row() >= m_messages.size())
        return {};

    const MsgInfo& msgInfo = m_messages[index.row()];

    switch(role) {
        case MsgDataRole: return msgInfo.msgData;
        case IsSelfRole: return msgInfo.isSelf;
        case SendStatusRole: return msgInfo.sendStatus;
        default: return {};
    }
}

QHash<int, QByteArray> ChatMsgModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[MsgDataRole] = "msgData";
    roles[IsSelfRole] = "isSelf";
    roles[SendStatusRole] = "sendStatus";
    return roles;
}

void ChatMsgModel::appendMsg(const MsgInfo &msgInfo)
{
    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append(msgInfo);
    endInsertRows();
}

void ChatMsgModel::appendList(const QList<MsgInfo> &msgInfoList)
{
    if (msgInfoList.isEmpty())
        return;

    int first = m_messages.size();
    int last = first + msgInfoList.size() - 1;

    beginInsertRows(QModelIndex(), first, last);
    for(const MsgInfo& msgInfo: msgInfoList) {
        m_messages.append(msgInfo);
    }
    endInsertRows();
}

void ChatMsgModel::prependList(const QList<MsgInfo> &msgInfoList)
{
    m_isFetching = false;

    if(msgInfoList.isEmpty()) {
        // 没有更多数据
        m_hasNoMore = true;
        return;
    }

    if(msgInfoList.size() < 20) {
        // 最后一页不足 20 条，没有更多数据
        m_hasNoMore = true;
    }

    int first = 0;
    int last = msgInfoList.size() - 1;

    beginInsertRows(QModelIndex(), first, last);
    for (int i = 0; i < msgInfoList.size(); ++i) {
        m_messages.insert(i, msgInfoList[i]);
    }
    endInsertRows();
}

void ChatMsgModel::updateMsgStatus(const QString &clientMsgId, int status)
{
    for (int i = m_messages.size() - 1; i >= 0; --i) { // 倒序找更快，因为刚发的消息在末尾
        if (m_messages[i].clientMsgId == clientMsgId) {
            m_messages[i].sendStatus = status;
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {SendStatusRole}); // 局部刷新 UI
            break;
        }
    }
}

qint64 ChatMsgModel::getFirstMsgCreateTime()
{
    if(m_messages.isEmpty()) {
        qDebug() << "第一次拉取历史数据";
        return 0;
    }
    return m_messages.first().createTime;
}

void ChatMsgModel::init(const QString &peerUid, int sessionType)
{
    m_peerUid = peerUid;
    m_sessionType = sessionType;
    m_messages.clear();
    m_isFetching = true;
    m_hasNoMore = false;

    emit sig_reqLoadHistory(m_peerUid, m_sessionType, getFirstMsgCreateTime(), 20);
}

void ChatMsgModel::clear()
{
    beginResetModel();
    m_messages.clear();
    endResetModel();
}

void ChatMsgModel::fetchMoreHistory()
{
    // 防抖与边界保护：正在拉取，或者已经没数据了，直接 return
    if (m_isFetching || m_hasNoMore) return;

    // 加锁保护
    m_isFetching = true;

    // 取当前列表里最老那条消息的时间戳，去查比它更老的数据
    qint64 oldestTime = getFirstMsgCreateTime();

    // 发送异步读取信号
    qDebug() << "[ChatMsgModel] 触顶，向本地 DB 请求更老的历史记录...";
    emit sig_reqLoadHistory(m_peerUid, m_sessionType, oldestTime, 20);
}

void ChatMsgModel::insertGapMessages(const QList<MsgInfo>& gapMsgs)
{
    qDebug() << "当前执行insertGapMessages逻辑";
    if (gapMsgs.isEmpty()) return;

    // 自定义排序规则
    auto cmp = [](const MsgInfo& a, const MsgInfo& b){
        return a.msgId.toULongLong() < b.msgId.toULongLong();
    };

    // 过滤去重，并提取出真正需要插入的有效消息
    QList<MsgInfo> validMsgs;
    for (const MsgInfo& newMsg : gapMsgs) {
        auto it = std::lower_bound(m_messages.begin(), m_messages.end(), newMsg,
        [](const MsgInfo& existing, const MsgInfo& incoming) {
            return existing.msgId.toULongLong() < incoming.msgId.toULongLong();
        });

        // 如果遇到了完全相等的包（极端重发情况），直接跳过
        if (it != m_messages.end() && it->msgId == newMsg.msgId) {
            continue;
        }

        validMsgs.append(newMsg);
    }

    if (validMsgs.isEmpty()) return;


    // 确保将要插入的数据本身是按序排列的 (ASC)
    if(!std::is_sorted(validMsgs.begin(), validMsgs.end(), cmp))
        std::sort(validMsgs.begin(), validMsgs.end(), cmp);

    // 智能分块批量插入 (Smart Batching)
    int i = 0;
    while (i < validMsgs.size()) {
        // 找到当前这块数据的插入起点
        auto it = std::lower_bound(m_messages.begin(), m_messages.end(), validMsgs[i], cmp);
        int startIndex = std::distance(m_messages.begin(), it);

        // 探测最多有多少个连续的包可以“一起”插入这个位置
        int j = i + 1;
        while (j < validMsgs.size()) {
            // 如果下一个消息的 ID 依然小于原本挡在前面的那个 it，说明它们属于同一个空洞
            if (it == m_messages.end() || validMsgs[j].msgId.toULongLong() < it->msgId.toULongLong())
                j++;
            else
                break; // 遇到了阻碍，当前连续块被打断
        }

        int count = j - i;

        // 核心性能优化：批量通知 UI 插入范围 [startIndex, startIndex + count - 1]
        beginInsertRows(QModelIndex(), startIndex, startIndex + count - 1);

        // 将这块连续的消息一次性塞入底层容器
        for (int k = 0; k < count; ++k) {
            m_messages.insert(startIndex + k, validMsgs[i + k]);
        }

        endInsertRows();

        // 游标推进到下一个空洞块
        i = j;
    }
}
