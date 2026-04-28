pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import llfcchat 1.0
import "../../basic"

Popup {
    id: popupRoot
    width: 380
    height: 500
    modal: true
    focus: true
    dim: true

    background: Rectangle {
        radius: 12
        color: "#F8F9FA" // 极其浅的灰底，让内部白卡片浮出来
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#33000000"
            shadowBlur: 1
            shadowVerticalOffset: 4
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200 }
        NumberAnimation { property: "scale"; from: 0.9; to: 1.0; duration: 200; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 150 }
        NumberAnimation { property: "scale"; from: 1.0; to: 0.9; duration: 150 }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // 顶部标题栏
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            Text {
                anchors.centerIn: parent
                text: qsTr("新的朋友")
                font.pixelSize: 18
                font.bold: true
                color: "#111111"
            }
            // 关闭按钮
            Button {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 30; height: 30
                text: "✕"
                flat: true
                onClicked: popupRoot.close()
            }
        }

        // 分割线
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#E5E5E5" }

        ListView {
            id: applyList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            topMargin: 12
            bottomMargin: 12

            add: Transition {
                NumberAnimation { property: "opacity"; from: 0; to: 1.0; duration: 250 }
                NumberAnimation { property: "y"; from: -50; duration: 250; easing.type: Easing.OutQuad }
            }

            move: Transition {
                NumberAnimation { properties: "y"; duration: 300; easing.type: Easing.OutQuart }
            }

            model: LogicMgr.friendApplyModel

            delegate: Item {
                id: del
                required property string uid
                required property string username
                required property string email
                required property string avatarUrl
                required property string greeting
                required property bool isSender
                required property int status
                required property var applyTime
                width: applyList.width - 24
                height: 72

                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: "#FFFFFF"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        // 头像
                        Rectangle {
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            radius: 24
                            color: "#E0E0E0"
                            Image {
                                anchors.fill: parent
                                source: del.avatarUrl === "" ? "qrc:/res/head_1.png" : del.avatarUrl
                                sourceSize.width: width
                                sourceSize.height: height
                            }
                        }

                        // 中间文字信息
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                text: del.username
                                font.pixelSize: 16
                                font.bold: true
                                color: "#333333"
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Text {
                                text: del.greeting === "" ? "请求添加你为好友" : del.greeting
                                font.pixelSize: 13
                                color: "#999999"
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        // 右侧状态机 (UI 根据底层状态自动变化)
                        // 状态 A/B：已添加 / 已拒绝 (静默文本)
                        Text {
                            visible: del.status === 1 || del.status === 2
                            text: del.status === 1 ? qsTr("已添加") : qsTr("已拒绝")
                            font.pixelSize: 14
                            color: "#B0B0B0"
                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        }

                        // 状态 C：等待验证 (我是发起方，对方没处理)
                        Text {
                            visible: del.status === 0 && del.isSender === true
                            text: qsTr("等待验证")
                            font.pixelSize: 14
                            color: "#B0B0B0"
                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        }

                        // 状态 D：去处理按钮 (对方发给我，等待我处理)
                        Button {
                            visible: del.status === 0 && del.isSender === false
                            text: qsTr("去处理")
                            Layout.preferredWidth: 64
                            Layout.preferredHeight: 32
                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                            contentItem: Text {
                                text: (parent as Button).text
                                color: "#FFFFFF"
                                font.pixelSize: 13
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 4
                                color: (parent as Button).pressed ? "#06AD56" : ((parent as Button).hovered ? "#08D369" : "#07C160")
                            }

                            onClicked: {
                                // 关闭弹窗
                                popupRoot.close()

                                let applyInfo =  {
                                    fromUid: del.uid,
                                    fromUsername: del.username,
                                    fromUserEmail: del.email,
                                    greeting: del.greeting
                                }

                                BasicConfig.switchToFriendVerifyPage(applyInfo)
                            }
                        }
                    }
                }
            }
        }
    }
}
