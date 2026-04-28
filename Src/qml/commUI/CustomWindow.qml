import QtQuick
import QtQuick.Controls
import QWindowKit 1.0
import QtQuick.Effects
import "../basic"
import "../ChatDialog"
import llfcchat 1.0

Window {
    property alias windowAgent: windowAgent
    property alias titleBar: titleBar
    property bool titleBarVisible: true
    // 定义一个自定义属性来控制当前状态，默认设置为通信状态 "chat"
    property string currentWindowState: "chat"
    id: window
    visible: true
    title: qsTr("llfcchat")
    color: "#F3F4F6"
    width: 400
    height: 600

    WindowAgent {
        id: windowAgent
    }
    Component.onCompleted: {
        console.log("窗口创建成功")
        windowAgent.setup(window);
        windowAgent.centralize();
    }

    onClosing: (closeEvent) => {
        console.log("窗口正在关闭，开始优雅清理资源...")

        // 1. 调用 C++ 的全局清理逻辑
        LogicMgr.logoutAndClean()

        // 2. 接受关闭事件，允许窗口正常销毁
        closeEvent.accepted = true
    }

    Rectangle {
        id: titleBar
        width: parent.width
        height: 40
        z: 10
        visible: window.titleBarVisible
        color: "transparent"

        Component.onCompleted: {
            console.log("titleBar的高度为: ", titleBar.height)
            windowAgent.setTitleBar(titleBar)
        }

        Rectangle {
            id: floatingIsland
            height: 32
            width: 104
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            radius: 16
            color: "#FFFFFF"
            border.color: "#E5E7EB"
            border.width: 1

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: "#1A000000"
            }

            Row {
                anchors.centerIn: parent
                spacing: 6

                Button {
                    id: minButton
                    width: 26
                    height: 26
                    background: Rectangle {
                        id: bgRect
                        anchors.fill: parent
                        color: minButton.pressed ? "#D6D6D6" : (minHover.hovered ? "#EEEEEE" : floatingIsland.color)
                        radius: 13
                        Behavior on color {
                            ColorAnimation {
                                duration: 150
                            }
                        }
                        HoverHandler {
                            id: minHover
                            cursorShape: hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }
                    }
                    contentItem: Image {
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        source: "qrc:/res/最小化.svg"
                        fillMode: Image.PreserveAspectFit
                    }
                    onClicked: window.showMinimized()
                    Component.onCompleted: windowAgent.setHitTestVisible(minButton)
                }

                Button {
                    id: maxButton
                    width: 26
                    height: 26
                    padding: 6
                    visible: window.currentWindowState !== "login"
                    background: Rectangle {
                        id: maxBgRect
                        anchors.fill: parent
                        color: maxButton.pressed ? "#D6D6D6" : (maxHover.hovered ? "#EEEEEE" : floatingIsland.color)
                        radius: 13
                        Behavior on color {
                            ColorAnimation {
                                duration: 150
                            }
                        }
                        HoverHandler {
                            id: maxHover
                            cursorShape: hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }
                    }
                    contentItem: Image {
                        source: window.visibility === Window.Maximized ? "qrc:/res/最大化.svg" : "qrc:/res/最大化 细.svg"
                        fillMode: Image.PreserveAspectFit
                    }
                    onClicked: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
                    Component.onCompleted: windowAgent.setHitTestVisible(maxButton)
                }

                Button {
                    id: closeButton
                    width: 26
                    height: 26
                    padding: 6

                    background: Rectangle {
                        id: closeBgRect
                        anchors.fill: parent
                        color: closeButton.pressed ? "#D6D6D6" : (closeHover.hovered ? "#A52A2A" : floatingIsland.color)
                        radius: 13
                        Behavior on color {
                            ColorAnimation {
                                duration: 150
                            }
                        }
                        HoverHandler {
                            id: closeHover
                            cursorShape: hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }
                    }
                    contentItem: Image {
                        source: closeHover.hovered ? "qrc:/res/close_white.svg" : "qrc:/res/close_black.svg"
                        fillMode: Image.PreserveAspectFit
                    }
                    onClicked: window.close()
                    Component.onCompleted: windowAgent.setHitTestVisible(closeButton)
                }
            }
        }

        NetworkStateBanner {
            id: netBanner
            z: 99
            netState: ChatMgr.netState
            visible: false
        }
    }

    StateGroup {
        id: windowStateGroup
        state: window.currentWindowState
        states: [
            State {
                name: "chat"
                PropertyChanges {
                    window {
                        width: 1350
                        height: 900
                        minimumWidth: 750
                        minimumHeight: 500
                    }
                    netBanner {
                        visible: true
                    }
                }
            },
            State {
                name: "login"
                // 登录状态：较小的窗口和较窄的标题栏
                PropertyChanges {
                    window {
                        width: 400
                        height: 600
                    }

                    floatingIsland {
                        width: 72
                    }

                    netBanner {
                        visible: false
                    }
                }
            }
        ]
        onStateChanged: {
            Qt.callLater(function() {
                windowAgent.centralize();
            });
        }
    }
}
