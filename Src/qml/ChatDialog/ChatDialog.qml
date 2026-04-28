pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../commUI"
import "../basic"
import "ChatDialogUI"
import llfcchat 1.0

Rectangle {
    id: chatDialog
    color: "transparent"

    // 整体的水平布局
    RowLayout {
        anchors.fill: parent
        spacing: 0

        ListModel {
            id: navModel
            ListElement { name: "消息"; iconText: "qrc:/res/消息.svg" }
            ListElement { name: "联系人"; iconText: "qrc:/res/好友2.svg" }
            ListElement { name: "文件"; iconText: "qrc:/res/邮箱.svg" }
        }
        // 第一项
        Item {
            Layout.minimumWidth: 76
            Layout.maximumWidth: 76
            Layout.fillHeight: true
            Rectangle {
                id: island
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 10
                anchors.bottomMargin: 20
                anchors.topMargin: 10
                radius: width / 2
                color: "#1E1F22"
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowBlur: 0.8
                    shadowColor: Qt.rgba(0, 0, 0, 0.2)
                    shadowVerticalOffset: 2
                }

                Rectangle {
                    id: avatarArea
                    anchors.top: island.top
                    anchors.topMargin: 5
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 44
                    height: 44
                    color: "transparent"
                    Image {
                        id: img
                        anchors.fill: parent
                        sourceSize.width: width * Screen.devicePixelRatio
                        sourceSize.height: height * Screen.devicePixelRatio
                        fillMode: Image.PreserveAspectCrop
                        source: "qrc:/res/姜饼人.svg"
                        visible: false
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Rectangle {
                        id: maskItem
                        anchors.fill: parent
                        radius: width / 2
                        color: "black"
                        visible: false
                        layer.enabled: true
                    }
                    MultiEffect {
                        anchors.fill: img
                        source: img
                        maskEnabled: true
                        maskSource: maskItem
                    }
                }
                // 分割线
                Rectangle {
                    width: 20
                    height: 2
                    radius: 1
                    color: "#333333"
                    anchors.top: avatarArea.bottom
                    anchors.topMargin: 15
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                // 灵动滑块
                Rectangle {
                    id: activeIndicator
                    width: 36
                    height: 36
                    radius: 18
                    color: Qt.rgba(0.4, 0.4, 0.2, 0.8)
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: navList.y + navList.currentIndex * 50 + 7
                    Behavior on y {
                        SpringAnimation { spring: 3.5; damping: 0.25 }
                    }
                    opacity: navList.currentIndex === -1 ? 0 : 1
                    Behavior on opacity {
                        NumberAnimation { duration: 200 }
                    }
                }
                ListView {
                    id: navList
                    spacing: 0
                    width: parent.width
                    height: count * 50
                    anchors.top: avatarArea.bottom
                    anchors.topMargin: 35
                    interactive: false
                    model: navModel
                    delegate: Item {
                        id: del
                        required property int index
                        required property string iconText
                        readonly property bool isSelected: navList.currentIndex === del.index
                        readonly property bool isHovered: hoverHandler.hovered
                        width: navList.width
                        height: 50
                        focus: true
                        // 图标展示区
                        Image {
                            anchors.centerIn: parent
                            source: del.iconText
                            width: 24
                            height: 24
                            sourceSize.width: width * Screen.devicePixelRatio
                            sourceSize.height: height * Screen.devicePixelRatio
                            // 状态区分：选中时全亮且稍微放大，未选中时半透明缩小
                            opacity: del.isSelected ? 1.0 : (del.isHovered ? 0.75 : 0.5)
                            scale: del.isSelected ? 1.15 : 1.0

                            // 状态切换时的微动画
                            Behavior on opacity { NumberAnimation { duration: 200 } }
                            Behavior on scale { NumberAnimation { duration: 200 } }

                        }
                        HoverHandler {
                            id: hoverHandler
                            cursorShape: hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }
                        TapHandler {
                            onTapped: {
                                navList.currentIndex = del.index
                            }
                        }

                        Rectangle {
                            id: redDot
                            width: LogicMgr.friendApplyModel.unreadCount > 9 ? 24 : 18
                            height: 18
                            radius: 9
                            color: "#FF3B30"

                            // 锚定在图标的右上角
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: 2
                            anchors.rightMargin: 2

                            // 只有数量 > 0 时才显示红点
                            visible: del.index === 1 && LogicMgr.friendApplyModel.unreadCount > 0
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(255, 59, 48, 0.4)
                                shadowBlur: 0.5
                            }
                            Text {
                                anchors.centerIn: parent
                                // 超过99显示 99+
                                text: LogicMgr.friendApplyModel.unreadCount > 99 ? "99+" : LogicMgr.friendApplyModel.unreadCount
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
                    onCurrentIndexChanged: {
                        if(currentIndex === 0) chatList.setCurrentPage(ChatList.Message)
                        else if(currentIndex === 1) chatList.setCurrentPage(ChatList.Friends)
                        else if(currentIndex === 2) chatList.setCurrentPage(ChatList.Email)
                    }
                }
                Item {
                    width: parent.width
                    height: 50
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 10
                    Image {
                        anchors.centerIn: parent
                        width: 24
                        height: 24
                        source: "qrc:/res/Setting.svg"
                        sourceSize.width: width
                        sourceSize.height: height
                        opacity: 1
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            // 触发设置页面逻辑
                            navList.currentIndex = -1 // 可选：取消中部列表的选中状态
                        }
                    }
                }
            }
        }
        // 第二项
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 10
                anchors.bottomMargin: 20
                anchors.rightMargin: 20
                radius: 16 // 大圆角
                color: "#FFFFFF"
                clip: true
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowBlur: 1.0
                    shadowColor: Qt.rgba(0, 0, 0, 0.05) // 非常淡的阴影
                    shadowVerticalOffset: 4
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                     // 第二项
                    Rectangle {
                        id: leftPanel
                        color: "transparent" // 背景由大岛接管
                        Layout.minimumWidth: 260
                        Layout.maximumWidth: 260
                        Layout.fillHeight: true
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0
                            // 搜索框
                            SearchItem {
                                id: searchItem
                                Layout.minimumHeight: 60
                                Layout.maximumHeight: 60
                                Layout.fillWidth: true
                                Layout.leftMargin: 10
                                Layout.rightMargin: 10
                            }

                            // 列表
                            ChatList {
                                id: chatList
                                onCurrentPageChanged: {
                                    if(currentPage === ChatList.Message) navList.currentIndex = 0
                                    else if(currentPage === ChatList.Friends) navList.currentIndex = 1
                                    else if(currentPage === ChatList.Email) navList.currentIndex = 2
                                }
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                            }
                        }
                    }
                    Rectangle {
                        Layout.minimumWidth: 1
                        Layout.maximumWidth: 1
                        Layout.fillHeight: true
                        color: "#F0F0F0" // 极其微弱的分割线，不抢夺视觉焦点
                    }
                    // 第三项
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.rightMargin: 5
                        color: "transparent"
                        ChatStackLayout {
                            anchors.fill: parent

                        }
                    }
                }
            }
        }
    }
}
