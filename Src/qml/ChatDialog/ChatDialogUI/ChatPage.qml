pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import "../../basic"
import "../../commUI"
import llfcchat 1.0

Item {
    id: chatPageRoot
    property string targetUid: ""
    property string targetName: "未选择会话"
    property string targetRemark: ""
    property int targetSessionType: 1

    // 记录是否是首次切入该会话
    property bool isInitialLoad: false

    // 根据外部传来的 uid，向 C++ 索要独立的聊天记录 Model
    onTargetUidChanged: {
        if(targetUid !== "") {
            // 标记：我们刚切进一个新会话
            isInitialLoad = true

            // 获取对应的独立会话 Model（包含离线消息和历史记录）
            chatDataList.model = ChatMgr.getChatModel(targetUid, targetSessionType)
        }
    }

    Connections {
        target: LogicMgr
        function onSig_receive_chat_msg(sessionKey) {
            // 如果发消息的人，正好是当前打开的聊天界面
            console.log("ChatMgr里的currentActiveKey: ", ChatMgr.currentActiveSessionKey, " QML里的currentSessionKey: ", sessionKey)
            if (ChatMgr.currentActiveSessionKey === sessionKey) {
                // 强制列表滚到底部，看最新消息
                Qt.callLater(chatDataList.positionViewAtEnd)
            }
        }
    }

    Rectangle {
        id: titleItem
        height: 60
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        topRightRadius: 16
        color: "#FFFFFF"
        Rectangle {
            width: parent.width;
            height: 1
            anchors.bottom: parent.bottom
            color: "#EAEAEA"
        }

        Label {
            id: titleLabel
            text: chatPageRoot.targetName
            font.pixelSize: 18
            font.family: BasicConfig.commFont
            font.bold: true
            anchors.centerIn: parent
        }
    }
    ListView {
        property int availableWidth: width - rightMargin
        property bool noMore: false
        // 用来保存上一个模型，方便断开连接
        property var currentChatModel: null
        id: chatDataList
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.top: titleItem.bottom
        anchors.bottom: toolWid.top
        contentWidth: availableWidth
        cacheBuffer: 120

        spacing: 15
        topMargin: 15
        bottomMargin: 15
        ScrollBar.vertical: ScrollBar {}



        // 定义处理信号的专用函数
        function handleNoMoreData() {
            chatDataList.noMore = true
        }

        onModelChanged: {
            // 每次切换模型时，先把提示文本隐藏，因为新会话可能还有历史记录
            chatDataList.noMore = false

            // 拔掉旧模型的信号线（极其重要，防止 LRU 缓存导致信号重叠）
            if (currentChatModel && currentChatModel.sig_noMoreData) {
                currentChatModel.sig_noMoreData.disconnect(chatDataList.handleNoMoreData)
            }

            // 记录新模型
            currentChatModel = chatDataList.model

            // 插上新模型的信号线
            if (currentChatModel && currentChatModel.sig_noMoreData) {
                currentChatModel.sig_noMoreData.connect(chatDataList.handleNoMoreData)
            }
        }

        onAtYBeginningChanged: {
            if (atYBeginning && model) {
                console.log("触发下拉加载历史...")
                model.fetchMoreHistory()
            }
        }

        onCountChanged: {
            // 如果是初次加载，且确实拉到了历史记录 (count > 0)
            if (chatPageRoot.isInitialLoad && count > 0) {
                // 消费掉这个标记，防止下拉加载更多时也乱滚到底部
                chatPageRoot.isInitialLoad = false

                // 等这 20 条消息的 UI 画完，再精准滚到底部
                Qt.callLater(chatDataList.positionViewAtEnd)
            }
        }

        header: Item {
            width: chatDataList.width
            height: noMoreTip.visible ? 40 : 0 // 有提示时撑开高度
            Behavior on height { NumberAnimation { duration: 200 } } // 丝滑的展开动画

            Text {
                id: noMoreTip
                visible: chatDataList.noMore // 默认隐藏，收到信号再显示
                text: qsTr("— 没有更多历史消息了 —")
                color: "#B0B0B0"
                font.pixelSize: 12
                anchors.centerIn: parent
            }
        }

        delegate: ChatDataListItem {
            required property int index
            required property string msgData
            required property bool isSelf
            required property int sendStatus
            username: isSelf ? UserMgr.username : chatPageRoot.targetName
            userIcon: isSelf ? "qrc:/res/head_4.png" : "qrc:/res/head_3.jpg"
            msg: msgData
            owner: isSelf
            status: sendStatus
        }
        clip: true
    }
    Rectangle {
        id: toolWid
        anchors.bottom: textAreaAndButtons.top
        anchors.bottomMargin: 2
        anchors.left: parent.left
        anchors.right: parent.right
        height: 35
        color: "#FFFFFF"
        Row {
            id: rowToolWid
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 15
            spacing: 15
            StateButton {
                id: emojiBtn
                width: 26
                height: 26
                hoveredUrl: "qrc:/res/smile_hover.png"
                selectedUrl: "qrc:/res/smile_press.png"
                regularUrl: "qrc:/res/smile.png"
            }
            StateButton {
                id: fileBtn
                width: 26
                height: 26
                hoveredUrl: "qrc:/res/filedir_hover.png"
                selectedUrl: "qrc:/res/filedir_press.png"
                regularUrl: "qrc:/res/filedir.png"
            }
        }
    }
    Rectangle {
        id: textAreaAndButtons
        color: "#FFFFFF"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        bottomRightRadius: 16
        height: 200
        ScrollView {
            id: chatTextAreaScrollArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: buttonsItem.top
            anchors.margins: 10
            clip: true
            TextArea {
                id: chatTextArea
                font.family: BasicConfig.commFont
                font.pixelSize: 16
                padding: 5
                background: Item {}
                wrapMode: TextArea.Wrap
                selectByMouse: true

                Keys.onPressed: (event) => {
                    if(event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                        if(event.modifiers & Qt.ShiftModifier) {
                            insert(cursorPosition, "\n")
                        }
                        else {
                            sendBtn.click()
                        }
                        event.accepted = true
                    }
                }
            }
        }

        Item {
            id: buttonsItem
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 50
            Row {
                id: buttonRow
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 15
                anchors.bottomMargin: 15
                spacing: 12
                StateButton {
                    id: recvBtn
                    width: 76
                    height: 32
                    contentItem: Label {
                        text: qsTr("接收")
                        color: "#666666"
                        font.pixelSize: 14
                        font.family: BasicConfig.commFont
                        horizontalAlignment: Label.AlignHCenter
                        verticalAlignment: Label.AlignVCenter
                        anchors.fill: parent
                    }
                    bgRadius: 4
                    bgColor: recvBtn.pressed ? "#D0D0D0" : (recvBtn.isHovered ? "#E0E0E0" : "#F0F0F0")
                }
                StateButton {
                    id: sendBtn
                    width: 76
                    height: 32
                    contentItem: Label {
                        text: qsTr("发送")
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.family: BasicConfig.commFont
                        horizontalAlignment: Label.AlignHCenter
                        verticalAlignment: Label.AlignVCenter
                        anchors.fill: parent
                    }
                    bgRadius: 4
                    bgColor: sendBtn.pressed ? "#06AD56" : (sendBtn.isHovered ? "#08D369" : "#07C160")
                    onClicked: {
                        let text = chatTextArea.text.trim()
                        if(text !== "") {
                            if (chatPageRoot.targetUid === "") {
                                console.log("请先在左侧选择一位聊天对象！")
                                return;
                            }

                            ChatMgr.sendChatMessage(chatPageRoot.targetUid, chatPageRoot.targetSessionType, text)
                            // 清空输入框
                            chatTextArea.clear()
                            // 焦点归位，继续输入
                            chatTextArea.forceActiveFocus()
                            // 列表强制滚到底部，展示刚发出的消息
                            // 同样套上 Qt.callLater
                            Qt.callLater(chatDataList.positionViewAtEnd)
                        }
                    }
                }
            }
        }
    }
}
