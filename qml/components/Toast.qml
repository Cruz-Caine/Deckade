import QtQuick

Rectangle {
    id: root

    property string message: ""
    property string level: "info"
    signal expired()

    width: Math.min(440, textItem.implicitWidth + 58)
    height: Math.max(48, textItem.implicitHeight + 24)
    radius: 10
    color: "#151b27ee"
    border.width: 1
    border.color: level === "error" ? theme.red : level === "warning" ? theme.gold : theme.borderStrong

    Row {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: root.level === "error" ? theme.red : root.level === "warning" ? theme.gold : theme.cyan
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: textItem
            width: root.width - 60
            text: root.message
            color: theme.text
            font.family: theme.bodyFamily
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Timer {
        interval: root.level === "error" ? 6000 : 4200
        running: true
        onTriggered: root.expired()
    }
}
