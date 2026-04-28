pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../basic"
import llfcchat 1.0
Item {
    id: root
    property alias currentIndex: searchListView.currentIndex
    Item {
        id: item
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 60
        Row {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 15
            Image {
                source: "qrc:/res/添加好友.svg"
                anchors.verticalCenter: parent.verticalCenter
                width: 36
                height: 36
            }
            Text {
                width: implicitWidth + 10
                font.family: BasicConfig.commFont
                font.pixelSize: 14
                font.bold: true
                text: qsTr("查找用户名/邮箱")
                anchors.verticalCenter: parent.verticalCenter
            }
            Button {
                icon.source: "qrc:/res/箭头右.svg"
                width: 36
                height: 36
                background: Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                }
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    ListView {
        id: searchListView
        anchors.top: item.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        model: LogicMgr.searchModel
        clip: true
        topMargin: 10
        bottomMargin: 10
        spacing: 4

        delegate: Item {
            id: delegateRoot
            required property string uid
            required property string username
            required property string email
            required property int index

            width: ListView.view.width
            height: 72
            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 4
                anchors.bottomMargin: 4

                HoverHandler { id: hoverHandler}
                TapHandler {
                    onTapped: {
                        searchListView.currentIndex = delegateRoot.index
                        BasicConfig.switchToFriendApplyPage(delegateRoot.uid, delegateRoot.username, delegateRoot.email)
                        console.log("当前切换到了用户:", delegateRoot.username, ":" , delegateRoot.uid)
                    }
                }

                radius: 12
                color: (hoverHandler.hovered || ListView.isCurrentItem) ? "#FFFFFF" : "transparent"
                border.color: ListView.isCurrentItem ? "#07C160" : (hoverHandler.hovered ? "#E5E7EB" : "transparent")
                border.width: ListView.isCurrentItem ? 2 : 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: 20
                        color: "#07C160"
                        Text {
                            anchors.centerIn: parent
                            // 取名字的第一个字符并大写
                            text: delegateRoot.username.length > 0 ? delegateRoot.username.charAt(0).toUpperCase() : "?"
                            color: "white"
                            font.bold: true
                            font.pixelSize: 18
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: delegateRoot.username
                            font.pixelSize: 16
                            font.bold: true
                            color: "#1E1F22"
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: delegateRoot.email
                            font.pixelSize: 13
                            color: "#888888"
                            Layout.fillWidth: true
                            elide: Text.ElideRight // 邮箱太长自动省略号
                        }
                    }
                    Image {
                        source: "qrc:/res/箭头右.svg"
                        sourceSize: Qt.size(20, 20)
                        opacity: (ListView.isCurrentItem || hoverHandler.hovered) ? 1.0 : 0.0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }
                }
            }
        }
    }
}
