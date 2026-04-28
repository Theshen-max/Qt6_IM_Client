import QtQuick
import QtQuick.Controls
import llfcchat 1.0

Item {
    id: searchItem
    property int maxTextLen: 30
    TextField {
        property bool isEditing: false
        id: searchField
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        background: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0.98, 0.98, 0.98, 1)
            border.width: 2
            border.color: "#f1f1f1"
        }
        maximumLength: searchItem.maxTextLen
        placeholderText: qsTr("搜索")
        leftPadding: searchIcon.width + 10
        rightPadding: clearIcon.width + 10
        Image {
            id: searchIcon
            source: "qrc:/res/search.png"
            height: parent.height - 5
            width: height
            anchors.left: parent.left
            anchors.leftMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
        }

        Button {
            id: clearIcon
            visible: parent.isEditing
            height: parent.height - 5
            width: height
            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.verticalCenter: parent.verticalCenter
            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
            }
            contentItem: Image {
                anchors.fill: parent
                source: searchField.isEditing ? "qrc:/res/close_search.png" : "qrc:/res/close_transparent.png"
            }

            onClicked: searchField.clear()

            HoverHandler {
                cursorShape: hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }

        // 防抖定时器
        // Timer {
        //     id: debounceTimer
        //     running: false
        //     interval: 300 // 停顿 300 毫秒后才执行搜索
        //     repeat: false
        //     onTriggered: {
        //         let keyword = searchField.text.trim()
        //         if (keyword.length > 0)
        //             // 告诉 C++ 去执行搜索
        //             UserMgr.search(keyword)
        //         else
        //             LogicMgr.clearSearch()
        //     }
        // }

        onAccepted: {
            LogicMgr.searchUser(searchField.text.trim())
        }

        onTextChanged: {
            isEditing = (searchField.text.length !== 0)
            // debounceTimer.restart()
        }

        onIsEditingChanged: {
            if(!isEditing) {
                if(navList.currentIndex === 0) chatList.setCurrentPage(ChatList.Message)
                else if(navList.currentIndex === 1) chatList.setCurrentPage(ChatList.Friends)
                else if(navList.currentIndex === 2) chatList.setCurrentPage(ChatList.Email)
                LogicMgr.clearSearch();
            }
            else
                chatList.setCurrentPage(ChatList.Search)
        }

        onActiveFocusChanged: {
            if(activeFocus)
                chatList.setCurrentPage(ChatList.Search)
        }
    }
}
