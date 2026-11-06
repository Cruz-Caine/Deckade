import QtQuick

Rectangle {
    id: root

    property string text: ""
    property color fill: theme.panelRaised
    property color line: theme.border
    property color foreground: theme.textMuted
    property bool strong: false
    property bool dot: false

    implicitWidth: content.implicitWidth + (dot ? 30 : 20)
    implicitHeight: 26
    radius: 8
    color: fill
    border.width: 1
    border.color: line

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            visible: root.dot
            width: 7
            height: 7
            radius: 4
            color: root.foreground
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: root.text
            color: root.foreground
            font.family: root.strong ? theme.displayFamily : theme.bodyFamily
            font.weight: root.strong ? Font.DemiBold : Font.Medium
            font.pixelSize: 12
            font.letterSpacing: root.strong ? 1.1 : 0.2
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
