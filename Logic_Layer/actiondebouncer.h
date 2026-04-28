#ifndef ACTIONDEBOUNCER_H
#define ACTIONDEBOUNCER_H

#include <QObject>
#include "global.h"

class ActionDebouncer : public QObject
{
    Q_OBJECT
public:
    explicit ActionDebouncer(QObject *parent = nullptr);

    bool tryExecute(const QString& actionId, int cooldownMs = 2000);

    void reset(const QString& actionId);

    bool isTimerActive(const QString& actionId);

signals:
    void sig_wait_for_reset();

private:
    QHash<QString, bool> _isRunning;

    QHash<QString, QTimer*> _timers;

    QHash<QString, int> _tryCounts;

    constexpr static int MAX_TRY_COUNT = 5;
};

#endif // ACTIONDEBOUNCER_H
