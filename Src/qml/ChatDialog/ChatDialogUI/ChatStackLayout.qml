import QtQuick
import QtQuick.Layouts
import "../../basic"
import llfcchat 1.0
StackLayout  {
    id: s
    enum Page {
        ChatPage = 0,
        FriendApplyPage = 1,
        FriendVerifyPage = 2
    }

    currentIndex: 0

    ChatPage {
        id: chatPage
    }

    FriendApplyPage {
        id: friendApplyPage
        onCancelBtnClicked: s.currentIndex = 0
    }

    FriendVerifyPage {
        id: friendVerifyPage
        onAuthHandled: s.currentIndex = 0
    }

    Connections {
        target: BasicConfig
        function onSwitchToFriendApplyPage(uid, username, email) {
            s.currentIndex = 1
            friendApplyPage.targetUid = uid
            friendApplyPage.targetUsername = username
            friendApplyPage.targetEmail = email
        }

        function onSwitchToFriendVerifyPage(latestApplyInfo) {
            s.currentIndex = 2
            friendVerifyPage.applicantUid = latestApplyInfo.fromUid
            friendVerifyPage.applicantUsername = latestApplyInfo.fromUsername
            friendVerifyPage.applicantEmail = latestApplyInfo.fromUserEmail
            friendVerifyPage.greetingMsg = latestApplyInfo.greeting
        }

        function onOpenChatSession(uid, sessionType, username, remark) {
            s.currentIndex = 0
            chatPage.targetUid = uid
            chatPage.targetSessionType = sessionType
            chatPage.targetName = username
            chatPage.targetRemark = remark
            ChatMgr.setCurrentActiveSession(uid, sessionType)
        }
    }
}
