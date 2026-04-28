#ifndef SENDRATELIMITER_H
#define SENDRATELIMITER_H
#include <QQueue>
#include <QDateTime>
class SendRateLimiter {
public:
    SendRateLimiter(int maxPerSec = 5);

    bool canSend();

    void clear();

private:
    int _maxPerSecond;

    QQueue<qint64> _sendTimestamps;
};

#endif // SENDRATELIMITER_H
