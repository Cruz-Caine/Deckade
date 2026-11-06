import QtQuick
import QtQuick.Controls

Item {
    id: root

    property bool focusable: true
    property bool navEnabled: true
    property string navGroup: "main"
    property bool hovered: mouse.containsMouse
    property color ringColor: theme.accent
    property real ringRadius: 14
    property bool ringOutside: true
    default property alias content: contentItem.data

    implicitWidth: contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight
    activeFocusOnTab: true

    signal activated()
    signal secondaryActivated()

    Item {
        id: contentItem
        anchors.fill: parent
    }

    Rectangle {
        id: glow
        anchors.fill: parent
        anchors.margins: root.ringOutside ? -6 : 0
        radius: root.ringRadius + 6
        color: "transparent"
        border.width: 8
        border.color: root.ringColor
        opacity: root.activeFocus ? 0.22 : 0
        visible: opacity > 0.01
        Behavior on opacity {
            NumberAnimation { duration: settings.reduceMotion ? 0 : 130 }
        }
    }

    Rectangle {
        id: ring
        anchors.fill: parent
        anchors.margins: root.ringOutside ? -2 : 1
        radius: root.ringRadius + 2
        color: "transparent"
        border.width: root.activeFocus ? 3 : 0
        border.color: root.ringColor
        opacity: root.activeFocus ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: settings.reduceMotion ? 0 : 110 }
        }
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
