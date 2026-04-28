import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import "../basic"
import llfcchat 1.0

Popup {
    enum TipStatus {
        Success = 0,
        CommonFailed,
        UserError,
        EmailFailed,
        PasswordFailed,
        ConfirmPasswdError,
        VerifyCodeError
    }
    id: registerPopup
    property string tipCurrentText: ""
    property var tipCurrentStatus: RegisterPopup.TipStatus.Success
    property var tipMap: ({})

    width: parent?.width ?? 1050
    height: parent?.height ?? 900
    modal: true
    closePolicy: Popup.NoAutoClose
    focus: true
    // 每次打开初始化，因为close只会hide窗口而非真正销毁
    onOpened: {
        tipMap = {}
        tipCurrentText = ""
        usertextField.text = ""
        emailTextField.text = ""
        passTextField.text = ""
        confirmTextField.text = ""
        valTextField.text = ""
        usertextField.forceActiveFocus()
        stackLayout.currentIndex = 0
    }

    Connections {
        target: LogicMgr
        function onSig_showTip_changed(str, status) {
            registerPopup.tipCurrentText = str
            if(status) registerPopup.tipCurrentStatus = RegisterPopup.TipStatus.Success
            else registerPopup.tipCurrentStatus = RegisterPopup.TipStatus.CommonFailed
        }
        function onSig_reg_mod_success() {
            finishedItem.count = 5
            finishedItemTimer.start()
            stackLayout.currentIndex = 1
        }
    }

    function updateTip() {
        let keys = Object.keys(tipMap)
        const len = keys.length
        if(len === 0) {
            tipCurrentText = ""
            tipCurrentStatus = RegisterPopup.TipStatus.Success
            return
        }

        let last = tipMap[keys[len - 1]]
        tipCurrentText = last.text
        tipCurrentStatus = last.status
    }

    function checkEmail() {
        return registerPopup.tipMap.hasOwnProperty("email")
    }
    background: Rectangle {
        anchors.fill: parent
        color: "#66000000"
    }
    contentItem: Item {
        anchors.fill: parent
        Rectangle {
            id: islandCard
            width: 340
            height: 520
            anchors.centerIn: parent
            color: "#FFFFFF"
            radius: 24
            MultiEffect {
                source: islandCard
                anchors.fill: islandCard
                shadowEnabled: true
                shadowColor: "#1A000000"
                shadowBlur: 0.3
                shadowVerticalOffset: 10
            }

            StackLayout {
                id: stackLayout
                anchors.fill: parent
                anchors.margins: 30
                currentIndex: 0
                // 主界面
                ColumnLayout {
                    spacing: 12
                    // 顶部标题
                    Label {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 10
                        text: qsTr("注册新账号")
                        font.family: BasicConfig.commFont
                        font.pixelSize: 22
                        font.bold: true
                        color: "#333333"
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        id: errTip
                        text: registerPopup.tipCurrentText
                        font.family: BasicConfig.commFont
                        font.pixelSize: 14
                        color: registerPopup.tipCurrentStatus === RegisterPopup.TipStatus.Success ? "#34C759" : "#FF3B30"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 20
                        verticalAlignment: Label.AlignVCenter
                        horizontalAlignment: Label.AlignHCenter
                        opacity: Number(text !== "") // 需要占位
                    }

                    component FieldBackground: Rectangle {
                        color: parent.activeFocus ? "#FFFFFF" : "#F3F4F6"
                        radius: 10
                        border.width: parent.activeFocus ? 2 : 1
                        border.color: parent.activeFocus ? "#007AFF" : "transparent"
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }

                    // 用户
                    TextField {
                        id: usertextField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        font.family: BasicConfig.commFont
                        font.pixelSize: 15
                        placeholderText: qsTr("用户名")
                        leftPadding: 16
                        background: FieldBackground { anchors.fill: parent }

                        onEditingFinished: {
                            let vaild = /^[A-Za-z0-9]+$/
                            if(!vaild.test(usertextField.text)) {
                                registerPopup.tipMap["username"] = {
                                    text: qsTr("用户名格式不对"),
                                    status: RegisterPopup.TipStatus.UserError
                                }
                            }
                            else
                                delete registerPopup.tipMap["username"]
                            registerPopup.updateTip()
                        }
                    }
                    // 邮箱
                    TextField {
                        id: emailTextField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        font.family: BasicConfig.commFont
                        font.pixelSize: 15
                        placeholderText: qsTr("邮箱地址")
                        leftPadding: 16
                        background: FieldBackground { anchors.fill: parent}
                        onEditingFinished: {
                            let vaild = /^[^\s@]+@[^\s@]+\.[^\s@]+$/
                            if(!vaild.test(emailTextField.text)) {
                                registerPopup.tipMap["email"] = {
                                    text: qsTr("邮箱不能为空或格式不对"),
                                    status: RegisterPopup.TipStatus.EmailFailed
                                }
                            }
                            else
                                delete registerPopup.tipMap["email"]
                            registerPopup.updateTip()
                        }
                    }
                    // 密码
                    Item {
                        id: passwdItem
                        property bool passwdVisible: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42

                        TextField {
                            id: passTextField
                            anchors.fill: parent
                            font.family: BasicConfig.commFont
                            font.pixelSize: 15
                            placeholderText: qsTr("设置密码 (6-15位)")
                            leftPadding: 16
                            rightPadding: 40
                            echoMode: passwdItem.passwdVisible ? TextInput.Normal : TextInput.Password
                            background: FieldBackground { anchors.fill: parent }

                            onEditingFinished: {
                                let valid = /^[A-Za-z0-9~!@#$%^&*]{6,15}$/
                                if(!valid.test(passTextField.text)) {
                                    registerPopup.tipMap["password"] = {
                                        text: qsTr("密码为空或格式错误(6~15位)"),
                                        status: RegisterPopup.TipStatus.PasswordFailed
                                    }
                                }
                                else
                                    delete registerPopup.tipMap["password"]
                                registerPopup.updateTip()
                            }
                        }
                        Image {
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            source: passwdHover.hovered ?
                                    (passwdItem.passwdVisible ? "qrc:/res/visible_hover.png" : "qrc:/res/unvisible_hover.png") :
                                    (passwdItem.passwdVisible ? "qrc:/res/visible.png" : "qrc:/res/unvisible.png")
                            Item {
                                anchors.fill: parent
                                anchors.margins: -5
                                HoverHandler {
                                    id: passwdHover
                                    cursorShape: Qt.PointingHandCursor
                                }
                                TapHandler {
                                    onTapped: {
                                        passwdItem.passwdVisible = !passwdItem.passwdVisible
                                    }
                                }
                            }
                        }
                    }
                    // 确认
                    Item {
                        id: confirmItem
                        property bool confirmVisible: false
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42

                        TextField {
                            id: confirmTextField
                            anchors.fill: parent
                            font.family: BasicConfig.commFont
                            font.pixelSize: 15
                            placeholderText: qsTr("确认密码")
                            leftPadding: 16
                            rightPadding: 40
                            echoMode: confirmItem.confirmVisible ? TextInput.Normal : TextInput.Password
                            background: FieldBackground {}

                            onEditingFinished: {
                                if(confirmTextField.text !== passTextField.text) {
                                    registerPopup.tipMap["confirm"] = {
                                        text: qsTr("确认密码与密码不一致"),
                                        status: RegisterPopup.TipStatus.ConfirmPasswdError
                                    }
                                }
                                else
                                    delete registerPopup.tipMap["confirm"]
                                registerPopup.updateTip()
                            }
                        }
                        Image {
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            source: confirmPasswdHover.hovered ?
                                    (confirmItem.confirmVisible ? "qrc:/res/visible_hover.png" : "qrc:/res/unvisible_hover.png") :
                                    (confirmItem.confirmVisible ? "qrc:/res/visible.png" : "qrc:/res/unvisible.png")
                            Item {
                                anchors.fill: parent
                                anchors.margins: -5
                                HoverHandler {
                                    id: confirmPasswdHover
                                    cursorShape: Qt.PointingHandCursor
                                }
                                TapHandler {
                                    onTapped: {
                                        confirmItem.confirmVisible = !confirmItem.confirmVisible
                                    }
                                }
                            }
                        }
                    }
                    // 验证码
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        spacing: 10

                        TextField {
                            id: valTextField
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            font.family: BasicConfig.commFont
                            font.pixelSize: 15
                            placeholderText: qsTr("验证码")
                            leftPadding: 16
                            background: FieldBackground { anchors.fill: parent }

                            onEditingFinished: {
                                let vaild = /^[A-Za-z0-9]{4}$/
                                if(!vaild.test(valTextField.text)) {
                                    registerPopup.tipMap["verifyCode"] = {
                                        text: qsTr("验证码为4位字母或数字"),
                                        status: RegisterPopup.TipStatus.VerifyCodeError
                                    }
                                }
                                else
                                    delete registerPopup.tipMap["verifyCode"]
                                registerPopup.updateTip()
                            }
                        }

                        GetVerifyCodeBtn {
                            id: button
                            Layout.preferredWidth: 100
                            Layout.fillHeight: true
                            onClicked: {
                                if(registerPopup.checkEmail()) {
                                    registerPopup.tipCurrentText = registerPopup.tipMap["email"]["text"];
                                    registerPopup.tipCurrentStatus = registerPopup.tipMap["email"]["status"]
                                } else {
                                    LogicMgr.send_varifyCode(emailTextField.text)
                                    startTimer()
                                }
                            }
                        }
                    }
                    // 空白填充
                    Item { Layout.preferredWidth: 1; Layout.fillHeight: true }
                    // 选择确认/取消
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        // 确认注册按钮
                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            contentItem: Text {
                                text: qsTr("确认注册")
                                font.family: BasicConfig.commFont
                                font.pixelSize: 16
                                font.bold: true
                                color: "#FFFFFF"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 22
                                color: confirmHover.hovered ? "#0066CC" : "#007AFF"
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                            HoverHandler { id: confirmHover; cursorShape: Qt.PointingHandCursor }

                            onClicked: {
                                usertextField.editingFinished()
                                emailTextField.editingFinished()
                                passTextField.editingFinished()
                                confirmTextField.editingFinished()

                                if(Object.keys(registerPopup.tipMap).length === 0) {
                                    LogicMgr.confirmBtn_clicked(usertextField.text, emailTextField.text, passTextField.text, confirmTextField.text, valTextField.text)
                                    return
                                }
                                registerPopup.updateTip()
                            }
                        }

                        // 取消/返回按钮
                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            contentItem: Text {
                                text: qsTr("返回登录")
                                font.family: BasicConfig.commFont
                                font.pixelSize: 15
                                color: cancelHover.hovered ? "#333333" : "#8E8E93"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                            background: Rectangle {
                                radius: 22
                                color: cancelHover.hovered ? "#F3F4F6" : islandCard.color
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                            HoverHandler { id: cancelHover; cursorShape: Qt.PointingHandCursor }

                            onClicked: {
                                registerPopup.close()
                            }
                        }
                    }
                    // 空白填充
                    Item { Layout.fillHeight: true; Layout.preferredWidth: 1 }
                }
                // 侧界面
                Item {
                    id: finishedItem
                    property int count: 5
                    ColumnLayout {
                        anchors.centerIn: parent
                        width: parent.width
                        spacing: 20
                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.minimumWidth: 60
                            Layout.maximumWidth: 60
                            Layout.minimumHeight: 60
                            Layout.maximumHeight: 60
                            radius: 30
                            color: "#E8F5E9"
                            border.color: "#34C759"
                            border.width: 2
                            Text {
                                anchors.centerIn: parent
                                text: "✓"
                                font.pixelSize: 36
                                color: "#34C759"
                            }
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("注册成功！")
                            font.family: BasicConfig.commFont
                            font.pixelSize: 22
                            font.bold: true
                            color: "#333333"
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("%1s 后自动返回登录界面").arg(finishedItem.count)
                            font.family: BasicConfig.commFont
                            font.pixelSize: 15
                            color: "#8E8E93"
                        }
                        Item {
                            Layout.preferredHeight: 10
                            Layout.preferredWidth: 1
                        }

                        Button {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 200
                            Layout.preferredHeight: 44
                            contentItem: Text {
                                text: qsTr("立即返回")
                                font.family: BasicConfig.commFont
                                font.pixelSize: 16
                                font.bold: true
                                color: "#FFFFFF"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 22
                                color: returnHover.hovered ? "#0066CC" : "#007AFF"
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                            HoverHandler { id: returnHover; cursorShape: Qt.PointingHandCursor }

                            onClicked: {
                                if(finishedItemTimer.running)
                                    finishedItemTimer.stop()
                                registerPopup.close()
                                BasicConfig.clearLoginShowTip()
                            }
                        }
                    }
                    Timer {
                        id: finishedItemTimer
                        running: false
                        interval: 1000
                        repeat: true

                        onTriggered: {
                            --finishedItem.count;
                            if(finishedItem.count <= 0) {
                                finishedItemTimer.stop()
                                registerPopup.close()
                                BasicConfig.clearLoginShowTip()
                            }
                        }
                    }
                 }
            }
        }
    }
}
