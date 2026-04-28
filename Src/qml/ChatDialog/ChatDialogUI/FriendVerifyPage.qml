pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../../basic" // 请确保路径与您的工程匹配
import llfcchat 1.0

Item {
    id: verifyPageRoot
    property string applicantUid: ""
    property string applicantUsername: "未知用户"
    property string applicantEmail: "unknown@example.com"
    property string applicantAvatar: "qrc:/res/head_5.png" // 可替换为实际头像路径
    property string greetingMsg: "你好，我是..." // 对方发来的验证消息

    // 信号透传，供外部堆栈视图控制页面跳转
    signal authHandled()

    Connections {
        target: LogicMgr
        function onSig_auth_rsp(rspInfo) {
            verifyPageRoot.authHandled()
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.8, 420) // 自定义大小，限制最大尺寸，避免过渡拉伸导致不美观
        spacing: 24

        // 标题区
        Text {
            text: qsTr("好友验证")
            font.pixelSize: 22
            font.bold: true
            font.family: BasicConfig.commFont
            color: "#1E1F22"
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 10
        }

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
                // 头像处理 (带圆形遮罩)
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
                        source: verifyPageRoot.applicantAvatar
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
                        text: verifyPageRoot.applicantUsername
                        font.pixelSize: 18
                        font.bold: true
                        color: "#1E1F22"
                    }
                    Text {
                        text: verifyPageRoot.applicantEmail
                        font.pixelSize: 13
                        color: "#888888"
                        elide: Text.ElideRight
                        Layout.maximumWidth: 260
                    }
                }
            }
        }
        ColumnLayout {
            spacing: 8
            Layout.fillWidth: true

            Text {
                text: qsTr("验证信息：")
                font.pixelSize: 14
                font.bold: true
                color: "#666666"
            }
            // 展示信息的悬浮卡片
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(80, msgText.implicitHeight + 30)
                radius: 8
                color: "#F8F9FA"
                border.color: "#EAEAEA"
                border.width: 1

                Text {
                    id: msgText
                    anchors.fill: parent
                    anchors.margins: 15
                    text: verifyPageRoot.greetingMsg
                    wrapMode: Text.Wrap
                    font.pixelSize: 15
                    color: "#333333"
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
        // 底部操作按钮区 (双选)
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 20
            spacing: 16

            // 拒绝按钮
            Button {
                id: rejectBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                hoverEnabled: true
                contentItem: Text {
                    text: qsTr("拒绝")
                    font.pixelSize: 15
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: "#666666"
                }
                background: Rectangle {
                    radius: 8
                    color: rejectBtn.pressed ? "#E0E0E0" : (rejectBtn.hovered ? "#EAEAEA" : "#F3F4F6")
                }
                onClicked: {
                    console.log("拒绝了用户: ", verifyPageRoot.applicantUid, " 的申请")

                    // 构建 JSON 并发送拒绝包
                    let reqData = {
                        "applicantUid": verifyPageRoot.applicantUid,
                        "action": "reject"
                    }

                    UserMgr.curApplicantUid = verifyPageRoot.applicantUid
                    UserMgr.curApplicantUsername = verifyPageRoot.applicantUsername
                    UserMgr.curApplicantUserEmail = verifyPageRoot.applicantEmail
                    LogicMgr.sendAuthFriendRequest(reqData)
                }
            }

            // 同意按钮
            Button {
                id: agreeBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                hoverEnabled: true
                contentItem: Text {
                    text: qsTr("同意")
                    font.pixelSize: 15
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: "#FFFFFF"
                }
                background: Rectangle {
                    radius: 8
                    color: agreeBtn.pressed ? "#06AD56" : (agreeBtn.hovered ? "#08D369" : "#07C160")
                }
                onClicked: {
                    console.log("同意了用户: ", verifyPageRoot.applicantUid, " 的申请")

                    // 构建 JSON 并发送同意包
                    let reqData = {
                        "applicantUid": verifyPageRoot.applicantUid,
                        "action": "agree"
                    }

                    UserMgr.curApplicantUid = verifyPageRoot.applicantUid
                    UserMgr.curApplicantUsername = verifyPageRoot.applicantUsername
                    UserMgr.curApplicantUserEmail = verifyPageRoot.applicantEmail
                    LogicMgr.sendAuthFriendRequest(reqData)
                }
            }
        }
    }
}
