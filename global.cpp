#include "global.h"

QString gate_url_prefix{};

QString getClientMsgId() {
    return QUuid::createUuidV7().toString(QUuid::WithoutBraces);
}
