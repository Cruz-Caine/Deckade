import QtQuick
import QtQuick.Controls

TextField {
    id: root

    property string navGroup: "main"
    property bool navEnabled: true
    property bool emphasized: false
    property bool deckTextInput: true

    height: 44
    leftPadding: 12
    rightPadding: 12
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    color: theme.text
    placeholderTextColor: theme.textFaint
    selectionColor: theme.accent
    selectedTextColor: "#ffffff"
    font.family: theme.bodyFamily
    font.pixelSize: 14
    selectByMouse: true

    background: Rectangle {
        radius: 8
        color: theme.panelRaised
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? theme.accent : root.emphasized ? theme.borderStrong : theme.border
        Behavior on border.color { ColorAnimation { duration: settings.reduceMotion ? 0 : 100 } }
    }

    Component.onCompleted: focusController.registerItem(root)
    Component.onDestruction: focusController.unregisterItem(root)
}
