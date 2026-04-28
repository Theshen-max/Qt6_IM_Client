pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import llfcchat 1.0

Item {
    id: rootRect
    ListView {
        id: listView
        anchors.fill: parent
        clip: true
        property alias isHovered: chatUserListHover.hovered
        property bool isLoading: false
        property var icons: ["qrc:/res/head_1.png", "qrc:/res/head_2.png", "qrc:/res/head_3.jpg", "qrc:/res/head_4.png", "qrc:/res/head_5.png", "qrc:/res/舔狗.png", "qrc:/res/传输-01.png", "qrc:/res/存钱罐-01.png"]
        property bool isAutoSelected: false
        rightMargin: 6
        spacing: 5

        ScrollBar.vertical: ScrollBar {
            policy: (listView.isHovered || scrollBarHoverHandler.hovered) ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            anchors.right: parent.right
            contentItem: Rectangle {
                implicitWidth: 6
                implicitHeight: 10
                radius: 3
                color: Qt.rgba(0.68, 0.66, 0.66, 1)
            }

            background: Rectangle {
                anchors.fill: parent
                color: "transparent"

                HoverHandler {
                    id: scrollBarHoverHandler
                }
            }
        }
        add: Transition {
                NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: 300 }
                NumberAnimation { property: "scale"; from: 0.8; to: 1.0; duration: 300; easing.type: Easing.OutBack }
            }

        // 移除好友时的缩小消失动画
        remove: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: 250 }
            NumberAnimation { property: "scale"; to: 0.5; duration: 250 }
        }

        model: ChatMgr.convModel
        delegate: ChatModeListItem {
            id: del
            required property string username
            required property string lastMsgContent
            required property string peerUid
            required property int sessionType
            required property int unreadCount

            width: listView.width - listView.rightMargin
            height: 70
            delUid: peerUid
            delUsername: username
            delUserIcon: listView.icons[Math.floor(Math.random() * listView.icons.length)]
            delUserChatMsg: lastMsgContent
            delSessionType: sessionType
            unreadCount: unreadCount
        }
        HoverHandler {
            id: chatUserListHover
        }
    }
}


