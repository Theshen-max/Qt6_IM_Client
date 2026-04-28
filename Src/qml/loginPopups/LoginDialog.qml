pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../basic"
import llfcchat 1.0

Item {
    id: root
    anchors.fill: parent
    property string errTipText: ""
    property bool errStatus: true // true: 无错误， false: 有错误

    Connections {
        target: BasicConfig
        function onClearLoginShowTip() {
            root.errTipText = ""
            root.errStatus = true
        }
    }

    Connections {
        target: LogicMgr
        function onSig_loginShowTip_changed(res, status) {
            root.errTipText = res
            root.errStatus = status
        }
    }

    Keys.onPressed: (event) => {

    }



    Rectangle {
        id: islandCard
        width: 350
        height: 560
        anchors.centerIn: parent
        color: "#FFFFFF"
        radius: 24
        visible: false
    }
    MultiEffect {
        source: islandCard
        anchors.fill: islandCard
        shadowEnabled: true
        shadowColor: "#1A000000"
        shadowBlur: 0.5
        shadowVerticalOffset: 10
        shadowHorizontalOffset: 0
        z: 1
    }
    ColumnLayout {
        anchors.fill: islandCard
        anchors.margins: 32
        spacing: 20
        z: 2
        // 顶部头像区域
        Item {
            id: avatarRoot
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            Layout.topMargin: 10
            property color pulseColor: "#E0E0E0"
            property int avatarSize: 90
            Repeater {
                id: rep
                model: 3
                delegate: Rectangle {
                    id: pulseCircle
                    required property int index
                    radius: width / 2
                    color: "#E0E0E0"
                    anchors.centerIn: parent
                    SequentialAnimation {
                        running: true
                        PauseAnimation {
                            duration: pulseCircle.index * 350
                        }
                        SequentialAnimation {
                            loops: Animation.Infinite
                            PropertyAction { target: pulseCircle; property: "width"; value: 90 }
                            PropertyAction { target: pulseCircle; property: "height"; value: 90 }
                            PropertyAction { target: pulseCircle; property: "opacity"; value: 0.6 }
                            ParallelAnimation {
                                NumberAnimation {
                                    target: pulseCircle
                                    properties: "width,height"
                                    to: 170 + (pulseCircle.index * 10)
                                    duration: 2500
                                    easing.type: Easing.OutCubic
                                }
                                NumberAnimation {
                                    target: pulseCircle
                                    property: "opacity"
                                    to: 0.0
                                    duration: 2500
                                    easing.type: Easing.OutCubic
                                }
                            }
                            PauseAnimation { duration: 500 }
                        }
                    }
                }
            }
            Rectangle {
                width: avatarRoot.avatarSize
                height: avatarRoot.avatarSize
                anchors.centerIn: parent
                color: "#FFFFFF"
                radius: width / 2
                z: 10
                border.color: "#F0F0F0"
                border.width: 1
                Image {
                    id: avatarImage
                    anchors.fill: parent
                    source: "qrc:/res/姜饼人.svg"
                    sourceSize.width: width * Screen.devicePixelRatio
                    sourceSize.height: height * Screen.devicePixelRatio
                    fillMode: Image.PreserveAspectCrop
                }
            }
        }
        // 表单输入区域
        Column {
            Layout.fillWidth: true
            Layout.topMargin: 10
            spacing: 16
            // 用户名输入框
            TextField {
                id: userEdit
                width: parent.width
                height: 46
                font.family: BasicConfig.commFont
                font.pixelSize: 16
                placeholderText: qsTr("请输入用户名")
                leftPadding: 16
                background: Rectangle {
                    color: userEdit.activeFocus ? "#FFFFFF" : "#F3F4F6"
                    radius: 12
                    border.width: userEdit.activeFocus ? 2 : 1
                    border.color: userEdit.activeFocus ? "#007AFF" : "transparent"
                    Behavior on border.color { ColorAnimation { duration: 200 } }
                    Behavior on color { ColorAnimation { duration: 200 } }
                }
                onTextEdited: {
                    root.errStatus = true
                    root.errTipText = ""
                }
            }
            // 密码输入框
            TextField {
                id: passEdit
                width: parent.width
                height: 46
                font.family: BasicConfig.commFont
                font.pixelSize: 16
                placeholderText: qsTr("请输入密码")
                echoMode: TextInput.Password
                leftPadding: 16
                background: Rectangle {
                    color: passEdit.activeFocus ? "#FFFFFF" : "#F3F4F6"
                    radius: 12
                    border.width: passEdit.activeFocus ? 2 : 1
                    border.color: passEdit.activeFocus ? "#007AFF" : "transparent"
                    Behavior on border.color { ColorAnimation { duration: 200 } }
                    Behavior on color { ColorAnimation { duration: 200 } }
                }
                onTextEdited: {
                    root.errStatus = true
                    root.errTipText = ""
                }
            }
            // 错误提示 + 忘记密码按钮
            Item {
                width: parent.width
                height: 20

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    font.family: BasicConfig.commFont
                    font.pixelSize: 15
                    text: root.errTipText
                    visible: !root.errStatus
                    color: "#FF3B30"
                }

                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    font.family: BasicConfig.commFont
                    font.pixelSize: 13
                    color: forgetHover.hovered ? "#007AFF" : "#8E8E93"
                    text: qsTr("忘记密码？")
                    HoverHandler { id: forgetHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        onTapped: {
                            console.log("跳转重置密码")
                        }
                    }
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
            }
        }
        // 底部操作按钮区域
        Column {
            Layout.fillWidth: true
            Layout.topMargin: 10
            spacing: 12

            // 登录按钮
            Button {
                id: loginBtn
                width: parent.width
                height: 46
                contentItem: Text {
                    text: qsTr("登 录")
                    font.family: BasicConfig.commFont
                    font.pixelSize: 16
                    font.bold: true
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 23
                    color: loginBtn.pressed ? "#005BB5" : (loginHover.hovered ? "#0066CC" : "#007AFF")
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                HoverHandler { id: loginHover; cursorShape: Qt.PointingHandCursor }
                onClicked: {
                    if(userEdit.text.trim() !== "" && passEdit.text.trim() !== "")
                        LogicMgr.loginBtn_clicked(userEdit.text, passEdit.text)
                    else {
                        root.errTipText = qsTr("用户名或者密码不能为空")
                        root.errStatus = false
                    }
                }
            }

            // 注册按钮
            Button {
                id: regBtn
                width: parent.width
                height: 46
                contentItem: Text {
                    text: qsTr("注册新账号")
                    font.family: BasicConfig.commFont
                    font.pixelSize: 16
                    font.bold: true
                    color: regHover.hovered ? "#007AFF" : "#333333"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                background: Rectangle {
                    radius: 23
                    color: regHover.hovered ? "#F3F4F6" : islandCard.color // 悬浮时出现浅灰背景
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
                HoverHandler { id: regHover; cursorShape: Qt.PointingHandCursor }
                onClicked: {
                    BasicConfig.openRegisterPopup()
                }
            }
        }

        // 占位弹簧，将内容往上推
        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
        }
    }
}
