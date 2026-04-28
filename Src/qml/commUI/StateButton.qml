import QtQuick
import QtQuick.Controls

Button {
    property alias isHovered: hoverHandler.hovered
    property url hoveredUrl
    property url selectedUrl
    property url regularUrl
    property alias bgColor: bg.color
    property alias bgRadius: bg.radius
    checkable: true
    icon.source: checked ? selectedUrl : (isHovered ? hoveredUrl : (regularUrl === undefined ? "" : regularUrl))
    icon.width: width * Screen.devicePixelRatio
    icon.height: height * Screen.devicePixelRatio
    HoverHandler {
        id: hoverHandler
        cursorShape: hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
    background: Rectangle {
        id: bg
        anchors.fill: parent
    }
}
