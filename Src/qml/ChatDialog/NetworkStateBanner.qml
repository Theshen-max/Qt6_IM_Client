import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

// 0: Disconnected, 1: Connecting, 2: Connected, 3: Reconnecting
Item {
    id: root
    width: parent.width
    height: 40

    // 暴露给外部绑定的网络状态
    property int netState: 0

    // 控制横幅的下拉与收起
    property bool isShow: false

    // 内部悬浮动画的 Y 轴偏移量
    property real floatOffsetY: 0

    // 状态配置字典 (空岛主题风格)
    property var stateConfigs: {
        0: { text: "岛屿失去引力，网络已断开", color: "#FF4D4F", icon: "🪨" },
        1: { text: "正在寻找航线...", color: "#1890FF", icon: "☁️" },
        2: { text: "已连接到空岛", color: "#52C41A", icon: "✨" },
        3: { text: "航线受阻，正在努力重连中...", color: "#FAAD14", icon: "🌪️" }
    }

    Timer {
        id: hideTimer
        interval: 2500 // 停留 2.5 秒
        repeat: false
        onTriggered: {
            root.isShow = false // 定时器触发，收起横幅
        }
    }

    // 注意，这里NetworkStateBanner在程序刚创建时就实例化了，只是state = "chat"时才visible: true
    onNetStateChanged: {
        if (netState === 2) {
            if (isShow) {
                hideTimer.start();
            }
        }
        else {
            hideTimer.stop();
            isShow = true;
        }
    }

    // 整体位置动画（下拉/收起）
    y: isShow ? 0 : -height - 10
    Behavior on y {
        NumberAnimation {
            duration: 500
            easing.type: Easing.OutBack // 带有弹性的坠落感
        }
    }

    // 主体横幅 (包含空岛悬浮浮动效果)
    Rectangle {
        id: bannerRect
        width: parent.width * 0.8
        height: 36
        anchors.horizontalCenter: parent.horizontalCenter

        // 叠加下拉坐标和内部悬浮坐标
        y: 3 + root.floatOffsetY
        radius: 18
        color: root.stateConfigs[root.netState]?.color || "#999999"
        opacity: 0.9 // 半透明，保留呼吸感

        // 阴影增加浮空感
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#40000000"
            shadowBlur: 1
            shadowVerticalOffset: 4
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 5
            spacing: 10

            // 状态图标
            Text {
                text: root.stateConfigs[root.netState]?.icon || ""
                font.pixelSize: 18
                Layout.leftMargin: 15
            }

            // 状态文字
            Text {
                text: root.stateConfigs[root.netState]?.text || ""
                color: "white"
                font.pixelSize: 14
                font.bold: true
                Layout.fillWidth: true
            }

            BusyIndicator {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                Layout.rightMargin: 15
                running: root.netState === 1 || root.netState === 3
                visible: running
            }
        }
    }

    SequentialAnimation {
        id: animation
        running: root.isShow
        loops: Animation.Infinite

        NumberAnimation {
            target: root
            property: "floatOffsetY"
            from: 0
            to: 6
            duration: 1500
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: root
            property: "floatOffsetY"
            from: 6
            to: 0
            duration: 1500
            easing.type: Easing.InOutSine
        }
    }
}
