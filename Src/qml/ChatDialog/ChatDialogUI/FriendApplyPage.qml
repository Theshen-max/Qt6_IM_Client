pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../basic"
import llfcchat 1.0

Item {
    id: applyPageRoot
    property string targetUid: ""
    property string targetUsername: "未知用户"
    property string targetEmail: "unknown@example.com"
    property string targetAvatar: "qrc:/res/姜饼人.svg"

    signal cancelBtnClicked()

    // 将整个表单居中显示，最大宽度限制为 400，防止在大屏幕上被无限拉伸变形
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.8, 420)
        spacing: 24

        // 标题
        Text {
            text: qsTr("申请添加联系人")
            font.pixelSize: 22
            font.bold: true
            font.family: BasicConfig.commFont
            color: "#1E1F22"
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 10
        }
        // 目标用户信息卡片
        Rectangle {
            Layout.fillWidth: true
            Layout.minimumHeight: 84
            Layout.maximumHeight: 84
            radius: 12
            color: "#F8F9FA"
            border.color: "#F0F0F0"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16
                // 头像处理
                Item {
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                    Rectangle {
                        id: maskShape
                        anchors.fill: parent
                        radius: width / 2
                        visible: false
                        layer.enabled: true
                    }
                    Image {
                        id: avatarImg
                        anchors.fill: parent
                        source: applyPageRoot.targetAvatar
                        sourceSize.width: width * Screen.devicePixelRatio
                        sourceSize.height: height * Screen.devicePixelRatio
                        fillMode: Image.PreserveAspectCrop
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            maskEnabled: true
                            maskSource: maskShape
                        }
                    }
                }

                // 用户名与邮箱
                ColumnLayout {
                    spacing: 4
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Text {
                        text: applyPageRoot.targetUsername
                        font.pixelSize: 18
                        font.bold: true
                        color: "#1E1F22"
                    }
                    Text {
                        text: applyPageRoot.targetEmail
                        font.pixelSize: 13
                        color: "#888888"
                        elide: Text.ElideRight // 邮箱太长自动省略号
                        Layout.maximumWidth: 260
                    }
                }
            }
        }
        // 验证信息
        ColumnLayout {
            spacing: 8
            Layout.fillWidth: true
            Text {
                text: qsTr("发送添加朋友申请：")
                font.pixelSize: 14
                font.bold: true
                color: "#666666"
            }
            TextArea {
                id: greetingInput
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                wrapMode: TextArea.Wrap
                font.pixelSize: 15
                // 默认填充自己的用户名
                text: qsTr("你好，我是 ") + BasicConfig.username

                background: Rectangle {
                    radius: 8
                    color: "#F8F9FA"
                    border.color: greetingInput.activeFocus ? "#07C160" : "transparent"
                    border.width: 1
                    layer.enabled: greetingInput.activeFocus
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Qt.rgba(7, 193, 96, 0.2)
                        shadowBlur: 0.6
                    }
                }
            }
        }

        // 备注信息
        ColumnLayout {
            spacing: 8
            Layout.fillWidth: true
            Text {
                text: qsTr("备注信息：")
                font.pixelSize: 14
                font.bold: true
                color: "#666666"
            }
            TextField {
                id: remarkInput
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                font.pixelSize: 15
                text: applyPageRoot.targetUsername

                background: Rectangle {
                    radius: 8
                    color: "#F8F9FA"
                    border.color: remarkInput.activeFocus ? "#07C160" : "transparent"
                    border.width: 1

                    layer.enabled: remarkInput.activeFocus
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Qt.rgba(7, 193, 96, 0.2)
                        shadowBlur: 0.6
                    }
                }
            }
        }

        // 底部操作按钮
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 20
            spacing: 16

            // 取消按钮
            Button {
                id: cancelBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                contentItem: Text {
                    text: qsTr("取消")
                    font.pixelSize: 15
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: "#666666"
                }
                background: Rectangle {
                    radius: 8
                    color: cancelBtn.pressed ? "#E0E0E0" : (cancelBtn.hovered ? "#EAEAEA" : "#F3F4F6")
                }
                onClicked: {
                    applyPageRoot.cancelBtnClicked()
                }
            }

            // 发送申请按钮
            Button {
                id: applyBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                contentItem: Text {
                    text: qsTr("发送申请")
                    font.pixelSize: 15
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: "#FFFFFF"
                }
                background: Rectangle {
                    radius: 8
                    color: applyBtn.pressed ? "#06AD56" : (applyBtn.hovered ? "#08D369" : "#07C160")
                }
                onClicked: {
                    console.log("向", applyPageRoot.targetUid, "发送好友申请")
                    console.log("打招呼：", greetingInput.text)
                    console.log("备注：", remarkInput.text)

                    let reqData = {
                        "targetUid": applyPageRoot.targetUid,
                        "greeting": greetingInput.text,
                        "remark": remarkInput.text
                    }
                    LogicMgr.sendAddFriendRequest(reqData)
                }
            }
        }
    }
}
