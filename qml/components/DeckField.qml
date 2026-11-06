import QtQuick
import QtQuick.Controls

TextField {
    id: root

    property string navGroup: "main"
    property bool navEnabled: true
    property bool emphasized: false
    property bool deckTextInput: true

    height: 54
    leftPadding: 18
    rightPadding: 18
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    color: theme.text
    placeholderTextColor: theme.textFaint
    selectionColor: theme.accent
    selectedTextColor: "#16070a"
    font.family: theme.bodyFamily
    font.pixelSize: 16
    selectByMouse: true

    background: Rectangle {
        radius: 10
        color: root.activeFocus ? theme.panelHover : theme.panel
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? theme.accent : root.emphasized ? theme.borderStrong : theme.border
        Behavior on border.color { ColorAnimation { duration: settings.reduceMotion ? 0 : 100 } }
    }

    Component.onCompleted: focusController.registerItem(root)
    Component.onDestruction: focusController.unregisterItem(root)
}
