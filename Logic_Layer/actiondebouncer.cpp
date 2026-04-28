#include "actiondebouncer.h"

ActionDebouncer::ActionDebouncer(QObject *parent)
    : QObject{parent}
{}

bool ActionDebouncer::tryExecute(const QString &actionId, int cooldownMs)
{
    if(++_tryCounts[actionId] > MAX_TRY_COUNT)
        // 通知QML，弹出Toast或者暂时禁用按键
        emit sig_wait_for_reset();

    // 检查标志量锁
    if(_isRunning.value(actionId, false))
        return false;

    _isRunning[actionId] = true;

    if(!_timers.contains(actionId)) {
        // 创建定时器并配置
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
         _timers[actionId] = timer;
        // 连接信号槽
        connect(timer, &QTimer::timeout, this, [this, actionId]{
            _isRunning[actionId] = false;
        });
    }

    // 启动定时器
    _timers[actionId]->start(cooldownMs);
    return true;
}

void ActionDebouncer::reset(const QString &actionId)
{
    _isRunning[actionId] = false;
    if (_timers.contains(actionId)) {
        _timers[actionId]->stop();
    }
}

bool ActionDebouncer::isTimerActive(const QString &actionId)
{
    if(!_timers.contains(actionId))
        return false;

    return _timers[actionId] && _timers[actionId]->isActive();
}
