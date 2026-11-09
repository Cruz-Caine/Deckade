import QtQuick
import QtQuick.Controls

Item {
    id: root

    property bool focusable: true
    property bool navEnabled: true
    property string navGroup: "main"
    property bool hovered: mouse.containsMouse
    property color ringColor: theme.accent
    property real ringRadius: 8
    property bool ringOutside: true
    // Labels the hint bar shows while this item is selected.
    property string hintPrimary: "Select"
    property string hintSecondary: ""
    property string hintTertiary: ""
    default property alias content: contentItem.data

    implicitWidth: contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight
    activeFocusOnTab: true

    signal activated()
    signal secondaryActivated()
    signal tertiaryActivated()

    Item {
        id: contentItem
        anchors.fill: parent
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.ringOutside ? -3 : 0
        radius: root.ringRadius + (root.ringOutside ? 3 : 0)
        color: "transparent"
        border.width: 2
        border.color: root.ringColor
        visible: root.activeFocus
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus(Qt.MouseFocusReason)
            root.activated()
        }
    }

    Keys.onReturnPressed: root.activated()
    Keys.onEnterPressed: root.activated()
    Keys.onSpacePressed: root.activated()

    Component.onCompleted: {
        if (root.focusable) {
            focusController.registerItem(root)
        }
    }
    Component.onDestruction: focusController.unregisterItem(root)

    onActiveFocusChanged: {
        if (activeFocus) {
            focusController.focusItem(root)
        }
    }
}
