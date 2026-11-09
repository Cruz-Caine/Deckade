import QtQuick

Rectangle {
    id: root

    property string text: ""
    property color fill: theme.panelRaised
    property color line: theme.border
    property color foreground: theme.textMuted
    property bool strong: false
    property bool dot: false

    implicitWidth: content.implicitWidth + 16
    implicitHeight: 22
    radius: 6
    color: theme.panelRaised

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 5

        Rectangle {
            visible: root.dot
            width: 6
            height: 6
            radius: 3
            color: root.foreground
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: root.text
            color: root.foreground
            font.family: theme.bodyFamily
            font.weight: Font.Medium
            font.pixelSize: 11
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
