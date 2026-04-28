import QtQuick
import QtQuick.Controls
import "../basic"


Button {
    id: button
    signal startTimer()
    property bool counting: false
    property int count: 10
    property int duration: 10
    property string normalText: qsTr("获取")
    enabled: !counting

    HoverHandler {
        id: hoverHandler
        cursorShape: hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    background: Rectangle {
        anchors.fill: parent
        border.width: 1
        border.color: hoverHandler.hovered ? "#409eff" : "#888888"
        radius: height / 2
        color: button.enabled ? (hoverHandler.hovered ? "#e6f2ff" : "#ffffff") : "#dddddd"
    }

    contentItem: Item {
        anchors.fill: parent
        Label {
            anchors.centerIn: parent
            text: button.counting ? String(button.count) : button.normalText
            font.family: BasicConfig.commFont
            font.pixelSize: 18
            font.bold: true
            color: "black"
        }
    }

    Timer {
        id: timer
        interval: 1000
        repeat: true
        running: false
        onTriggered: {
            --button.count
            if(button.count <= 0) {
                timer.stop()
                button.counting = false
            }
        }
    }

    onStartTimer: {
        button.count = button.duration
        button.counting = true
        timer.start()
    }
}
