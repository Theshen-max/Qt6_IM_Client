import QtQuick
import QtQuick.Controls
import "../../basic"
import llfcchat 1.0
Item {
    id: root
    property bool owner: true
    property string username: ""
    property string msg: ""
    property url userIcon: ""
    property int status: 1 // 发送状态属性 (0: 发送中, 1: 成功, 2: 失败)

    width: ListView.view?.width || parent.width
    height: Math.max(avatar.height, nameText.height + msgRect.height + 5) + 20
    // 头像
    Rectangle {
        id: avatar
        width: 40
        height: 40
        radius: 4
        clip: true
        color: "#CCCCCC" // 如果 userIcon 加载失败或为空的占位色

        // 动态锚点：自己靠右，对方靠左
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.left: root.owner ? undefined : parent.left
        anchors.right: root.owner ? parent.right : undefined
        anchors.leftMargin: 5
        anchors.rightMargin: 5

        Image {
            anchors.fill: parent
            source: root.userIcon
            sourceSize.width: width * Screen.devicePixelRatio
            sourceSize.height: height * Screen.devicePixelRatio
            fillMode: Image.PreserveAspectCrop
        }
    }

    // 名字
    Text {
        id: nameText
        text: root.username
        font.pixelSize: 12
        color: "#888888"

        anchors.top: avatar.top
        anchors.left: root.owner ? undefined : avatar.right
        anchors.right: root.owner ? avatar.left : undefined
        anchors.leftMargin: 10
        anchors.rightMargin: 10
    }

    // 消息气泡
    Rectangle {
        id: msgRect
        anchors.top: nameText.bottom
        anchors.topMargin: 4
        anchors.left: root.owner ? undefined : nameText.left
        anchors.right: root.owner ? nameText.right : undefined

        width: Math.min(msgText.implicitWidth + 24, root.width * 0.65) // 自适应大小
        height: Math.max(msgText.implicitHeight + 20, 35)
        radius: 8
        color: root.owner ? "#95EC69" : "#FFFFFF"

        TextEdit {
            id: msgText
            anchors.fill: parent
            anchors.margins: 10
            font.pixelSize: 16
            font.family: BasicConfig.commFont
            color: "#000000"
            verticalAlignment: TextEdit.AlignTop
            horizontalAlignment: TextEdit.AlignLeft
            wrapMode: Text.Wrap
            text: root.msg
            readOnly: true
            selectByMouse: true
            selectionColor: "#3399FF"   // 选中区域的颜色
            selectedTextColor: "#FFFFFF"    // 选中文字的颜色
        }

        Rectangle {
            width: 10
            height: 10
            anchors.left: root.owner ? undefined : msgRect.left
            anchors.right: root.owner ? msgRect.right : undefined
            anchors.top: msgRect.top
            anchors.topMargin: height + 1
            anchors.leftMargin: -width/2 + 1
            anchors.rightMargin: -width/2 + 1
            rotation: 45
            z: -1
            antialiasing: true
            color: msgRect.color
        }

        // 正三角
        // Shape {
        //     id: shape
        //     width: 20
        //     height: 20
        //     anchors.left: root.owner ? undefined : msgRect.left
        //     anchors.right: root.owner ? msgRect.right : undefined
        //     anchors.verticalCenter: msgRect.verticalCenter
        //     anchors.leftMargin: -width/2
        //     anchors.rightMargin: -width/2
        //     z: -1
        //     antialiasing: true
        //     ShapePath {
        //         strokeWidth: 0
        //         fillColor: msgRect.color

        //         startX: shape.width / 2
        //         startY: shape.height / 4
        //         PathLine {
        //             x: shape.width + shape.width / 4
        //             y: shape.height / 4
        //         }
        //         PathLine {
        //             x: shape.width / 2
        //             y: shape.height / 4 * 3
        //         }
        //     }
        // }
    }

    // 状态指示器 (转圈圈 / 红色感叹号)
    Item {
        id: statusIndicator
        width: 20
        height: 20
        // 只有是自己发的消息，且状态不是 1(成功) 时才显示
        visible: root.owner && root.status !== 1

        // 锚定在消息气泡的左边
        anchors.right: msgRect.left
        anchors.rightMargin: 10
        anchors.verticalCenter: msgRect.verticalCenter

        // 发送中状态: 菊花转圈 (status === 0)
        BusyIndicator {
            anchors.fill: parent
            visible: root.status === 0
            running: visible
        }

        // 发送失败状态: 红色感叹号 (status === 2)
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "#FF4D4F"  // 经典的错误警告红
            visible: root.status === 2

            Text {
                text: "!"
                color: "#FFFFFF"
                font.bold: true
                font.pixelSize: 14
                font.family: BasicConfig.commFont
                anchors.centerIn: parent
            }

            // TapHandler {
            //     onTapped: ChatMgr.xxx()
            // }
        }
    }
}
