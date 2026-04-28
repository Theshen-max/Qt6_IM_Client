import QtQuick
import QtQuick.Layouts

Item {
    enum Type{
        Horizontal,
        Vertical
    }
    id: spacerItem
    property var orientation: SpacerItem.Vertical
    Layout.preferredWidth: 1
    Layout.fillHeight: true

    states: State {
        name: "Horizontal"
        when: orientation === SpacerItem.Horizontal
        PropertyChanges {
            target: spacerItem
            Layout.preferfedHeight: 1
            Layout.fillWidth: true
        }
    }
}
