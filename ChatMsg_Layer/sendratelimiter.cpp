#include "sendratelimiter.h"

SendRateLimiter::SendRateLimiter(int maxPerSec) : _maxPerSecond(5)
{

}

bool SendRateLimiter::canSend()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 把队列中所有早于 1 秒前的时间戳全部踢掉
    while (!_sendTimestamps.isEmpty() && (now - _sendTimestamps.head() > 1000)) {
        _sendTimestamps.dequeue();
    }

    // 判断当前1秒内的发送量是否达到阈值
    if (_sendTimestamps.size() >= _maxPerSecond) {
        return false;
    }

    // 允许发送，记录当前时间戳
    _sendTimestamps.enqueue(now);
    return true;
}

void SendRateLimiter::clear()
{
    _sendTimestamps.clear();
}
