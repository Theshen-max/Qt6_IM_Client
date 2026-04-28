pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import llfcchat 1.0
import "../../basic"

Item {
    ListView {
        id: contactListView
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12
        clip: true

        header: Item {
            width: contactListView.width
            height: 84 // 给阴影和间距留出空间
            z: 2       // 确保阴影不会被下面的列表项遮挡

            Rectangle {
                id: newFriendCard
                anchors.fill: parent
                anchors.margins: 6
                anchors.bottomMargin: 14 // 底部留白，与联系人列表隔开
                radius: 12
                color: "#FFFFFF"

                // 悬空岛屿核心：弥散阴影
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: "#1A000000"
                    shadowBlur: 1
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 2
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    // 左侧图标 (可以放一个橘色背景的“+”号小人图标)
                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 8
                        color: "#FF9500"
                        Image {
                            anchors.centerIn: parent
                            source: "qrc:/res/添加好友.png" // 替换为你的图标路径
                            width: 24
                            height: 24
                            sourceSize.width: width
                            sourceSize.height: height
                        }
                    }

                    Text {
                        text: qsTr("新的朋友")
                        font.pixelSize: 16
                        font.bold: true
                        font.family: BasicConfig.commFont
                        color: "#333333"
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        visible: LogicMgr.friendApplyModel.unreadCount > 0
                        Layout.preferredWidth: Math.max(20, unreadText.contentWidth + 10)
                        Layout.preferredHeight: 20
                        radius: 10
                        color: "#FF3B30"

                        Text {
                            id: unreadText
                            anchors.centerIn: parent
                            text: LogicMgr.friendApplyModel.unreadCount > 99 ? "99+" : LogicMgr.friendApplyModel.unreadCount
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }
                }

                // 鼠标交互与点击事件
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: {
                        LogicMgr.friendApplyModel.clearUnreadCount()
                        applyPopup.open()
                    }
                }
            }
        }

        // 声明弹窗实例 (懒加载)
        FriendApplyPopup {
            id: applyPopup
            // 让弹窗居中于整个聊天主窗口，而不是当前小小的列表内
            parent: Overlay.overlay
            x: Math.round((parent.width - width) / 2)
            y: Math.round((parent.height - height) / 2)
        }


        model: UserMgr.friendModel
        add: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: 300 }
            NumberAnimation { property: "scale"; from: 0.8; to: 1.0; duration: 300; easing.type: Easing.OutBack }
        }

        remove: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0; duration: 300 }
            NumberAnimation { property: "scale"; from: 1.0; to: 0.8; duration: 300; easing.type: Easing.OutBack }
        }

        delegate: Item {
            required property string uid
            required property string username
            required property string email
            required property string remark
            required property string avatarUrl
            required property int status
            required property int index
            id: del
            width: contactListView.width
            height: 72

            Rectangle {
                id: cardBg
                anchors.fill: parent
                radius: 12
                color: hoverHandler.hovered ? "#F8FAFC" : "#FFFFFF"
                border.color: "#E2E8F0"
                border.width: 1
                // 岛屿阴影效果
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: "#1A000000" // 10% 透明度的纯黑
                    shadowBlur: hoverHandler.hovered ? 12 : 6
                    shadowVerticalOffset: hoverHandler.hovered ? 4 : 2
                    Behavior on shadowBlur { NumberAnimation { duration: 150 } }
                    Behavior on shadowVerticalOffset { NumberAnimation { duration: 150 } }
                }
                // 默认文字头像（取名字第一个字）
                Rectangle {
                    id: avatar
                    width: 46; height: 46
                    radius: 23
                    color: "#07C160"
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: del.username.charAt(0).toUpperCase()
                        color: "#FFFFFF"
                        font.pixelSize: 20
                        font.bold: true
                        font.family: BasicConfig.commFont
                    }
                }
                // 名字
                Text {
                    text: del.remark === "" ? del.username : del.remark
                    font.pixelSize: 16
                    font.bold: true
                    font.family: BasicConfig.commFont
                    color: "#333333"
                    anchors.left: avatar.right
                    anchors.leftMargin: 12
                    anchors.top: avatar.top
                    anchors.topMargin: 2
                }

                // 邮箱 / 个性签名
                Text {
                    text: del.email === "" ? "这个人很懒，没有留下邮箱" : del.email
                    font.pixelSize: 13
                    font.family: BasicConfig.commFont
                    color: "#999999"
                    anchors.left: avatar.right
                    anchors.leftMargin: 12
                    anchors.bottom: avatar.bottom
                    anchors.bottomMargin: 2
                }

                HoverHandler { id: hoverHandler }
                TapHandler {
                    onTapped: {
                        BasicConfig.openChatSession(del.uid, 1, del.username, del.remark)
                    }
                }
            }
        }
    }
}
