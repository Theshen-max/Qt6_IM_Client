pragma Singleton

import QtQuick

QtObject {
    readonly property string commFont: "微软雅黑 Light"
    property string username: "Yzc"
    property url userIcon: ""

    // 信号
    signal openRegisterPopup()
    signal switchChatDialog()
    signal switchToFriendApplyPage(uid: string, username: string, email: string)
    signal switchToFriendVerifyPage(latestApplyInfo: var)
    signal clearLoginShowTip()

    // 自定义信号：通知打开与谁的聊天
    signal openChatSession(string uid, int sessionType, string username, string remark)

    Component.onCompleted:
    {
        console.log(`===============================================
        当前屏幕分辨率是${Screen.width}x${Screen.height}
====================================================\n`)
    }

    // 测试
    readonly property color surface: "#0e0e11"
    readonly property color surfaceContainerLow: "#131316"
    readonly property color primary: "#ff80e4"
    readonly property color secondary: "#00eefc"

    // 文本颜色
    readonly property color textMain: "#f0edf1"
    readonly property color textSub: "#acaaae"

    // 玻璃卡片属性
    readonly property color glassBg: Qt.rgba(37/255, 37/255, 42/255, 0.3)
    readonly property color glassBorder: Qt.rgba(240/255, 237/255, 241/255, 0.1)

    // 字体名称 (需要先在 main.cpp 中加载 ttf/otf 文件)
    readonly property string fontHeadline: "Space Grotesk"
    readonly property string fontBody: "Manrope"
    property FontLoader manrope: FontLoader {
            source: "qrc:/font/Manrope-Regular.ttf"
        }
}
