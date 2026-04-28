pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import "../../basic"
import llfcchat 1.0

ItemDelegate {
    id: itemDel
    required property string delUid
    property alias delUserIcon: icon.source
    property string delUsername: ""
    property string delRemark: ""
    property string delUserChatMsg: ""
    property int delSessionType: 1
    property bool isSelected: ListView.isCurrentItem // 选中状态
    property bool isHovered: hoverHandler.hovered
    property int unreadCount: 0
    required property int index

    HoverHandler {
        id: hoverHandler
        cursorShape: hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    TapHandler {
        onTapped: {
            itemDel.ListView.view.currentIndex = itemDel.index
            BasicConfig.openChatSession(itemDel.delUid, itemDel.delSessionType, itemDel.delUsername, itemDel.delRemark)
            console.log("当前User的Uid: ", itemDel.delUid, "当前User的sesstionType: ", itemDel.delSessionType, " Username: ", itemDel.delUsername)
        }
    }

    // Component.onCompleted: {
    //     // 如果我是第一个元素，并且整个 ListView 还没有进行过自动选择
    //     if (itemDel.index === 0 && !itemDel.ListView.view.isAutoSelected) {
    //         itemDel.ListView.view.isAutoSelected = true // 标记为已选择

    //         // 执行和被点击一模一样的逻辑
    //         itemDel.ListView.view.currentIndex = itemDel.index
    //         BasicConfig.openChatSession(itemDel.delUid, itemDel.delSessionType, itemDel.delUsername, itemDel.delRemark)

    //         console.log("自动选中首个好友 Uid: ", itemDel.delUid, " Username: ", itemDel.delUsername)
    //     }
    // }

    contentItem: Item {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        Item {
            id: iconItem
            width: 48
            height: 48
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 5
            Image {
                id: icon
                anchors.fill: parent
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
            Rectangle {
                id: redDot
                width: itemDel.unreadCount > 9 ? 24 : 18
                height: 18
                radius: 9
                color: "#FF3B30"
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: -4
                anchors.rightMargin: -4
                visible: itemDel.unreadCount > 0 // 有未读才显示

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(255, 59, 48, 0.4)
                    shadowBlur: 0.5
                }

                Text {
                    anchors.centerIn: parent
                    text: itemDel.unreadCount > 99 ? "99+" : itemDel.unreadCount
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                    font.family: BasicConfig.commFont
                }

                Behavior on visible {
                    NumberAnimation {
                        target: redDot
                        property: "scale";
                        from: 0;
                        to: 1;
                        duration: 250;
                        easing.type: Easing.OutBack
                    }
                }
            }

        }
        Item{
            id: userInfoItem
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: iconItem.right
            anchors.leftMargin: 5
            anchors.right: timeItem.left
            anchors.rightMargin: 5
            Item {
                anchors.fill: parent
                anchors.margins: 2
                Label {
                    id: userNameLab
                    text: itemDel.delRemark === "" ? itemDel.delUsername : itemDel.delRemark
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    font.family: BasicConfig.commFont
                    font.pixelSize: 14
                }
                Label {
                    id: userChatLab
                    text: itemDel.delUserChatMsg
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    font.pixelSize: 12
                    color: Qt.rgba(0.6, 0.6, 0.6, 1)
                    elide: Label.ElideRight
                }
            }
        }
        Item{
            id: timeItem
            width: 50
            height: 50
            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            Label {
                id: timeLab
                text: "13:54"
                anchors.centerIn: parent
                font.pixelSize: 12
                color: Qt.rgba(0.55, 0.55, 0.55, 1)
            }
        }
    }

    background: Rectangle {
        id: bgRect
        color: {
            if (itemDel.isSelected) return "#C4C4C4"      // 选中时的深灰色
            if (itemDel.isHovered) return "#EBEBEB"       // 鼠标悬停时的浅灰色
            return "transparent"                          // 正常状态
        }
    }
}
