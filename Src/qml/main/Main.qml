/******************************************************************************
 *
 * @file       Main.qml
 * @brief      主窗口
 *
 * @author     YZC
 * @date       2026/03/01
 * @history
 *****************************************************************************/

import QtQuick
import "../commUI"
import "../ChatDialog"
import "../loginPopups"
import "../basic"
import llfcchat 1.0

CustomWindow {
    id: window
    visible: true
    title: qsTr("llfcchat")


    Connections {
        target: BasicConfig
        function onOpenRegisterPopup() {
            registerPopup.open()
        }
    }

    Connections {
        target: LogicMgr
        function onSig_switch_chatDialog() {
            window.currentWindowState = "chat"
        }

        function onSig_add_friend_success(notifyInfo) {
            let name = notifyInfo.friendName
            console.log("天大的好消息：", name, " 已经同意了您的好友申请！")

            // TODO: 调用全局的 Toast 组件弹出提示
            // GlobalToast.showSuccess(name + " 已添加您为好友，现在可以开始聊天了！")
        }
    }

    currentWindowState: "login"

    ChatDialog {
        id: chatDialog
        anchors.left: window.contentItem.left
        anchors.right: window.contentItem.right
        anchors.top: window.titleBar.bottom
        anchors.bottom: window.contentItem.bottom
        visible: window.currentWindowState === "chat"
    }

    LoginDialog {
        id: loginDialog
        anchors.fill: parent
        anchors.topMargin: 10
        visible: window.currentWindowState === "login"
    }

    RegisterPopup {
        id: registerPopup
        width: loginDialog.width
        height: loginDialog.height
    }
}
