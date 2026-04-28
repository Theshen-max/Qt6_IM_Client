import QtQuick
import QtQuick.Layouts

StackLayout {
    id: chatUserList

    enum Page {
        Message = 0,
        Friends,
        Email,
        Search
    }
    property var currentPage: ChatList.Message

    function setCurrentPage(page) {
        currentPage = page
    }

    onCurrentPageChanged: {
        switch(currentPage) {
            case ChatList.Message: currentIndex = 0; break;
            case ChatList.Friends: currentIndex = 1; break;
            case ChatList.Email: currentIndex = 2; break;
            case ChatList.Search: currentIndex = 3; break;
        }
    }

    Component.onCompleted: {
        currentIndex = 0
    }

    // 聊天模式
    ChatModeList{} // index === 0

    // 联系人模式
    ContactModeList { // index === 1
    }

    Item {}

    // 搜索模式
    SearchModeList {
    }   // index === 3
}
