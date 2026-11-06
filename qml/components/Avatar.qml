import QtQuick

Item {
    id: root
    property string hash: ""
    property string name: ""
    property int size: 40
    property color fallbackColor: theme.panelRaised
    property bool online: true

    width: size
    height: size

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: root.fallbackColor
        border.width: 1
        border.color: theme.border

        Image {
            anchors.fill: parent
            anchors.margins: 1
            source: root.hash.length > 0
                    ? "https://www.gravatar.com/avatar/" + root.hash + "?s=96&d=retro&r=g"
                    : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            visible: status === Image.Ready
            layer.enabled: true
            layer.effect: null
        }

        Text {
            anchors.centerIn: parent
            visible: root.hash.length === 0
            text: (root.name || "?").slice(0, 1).toUpperCase()
            color: theme.textMuted
            font.family: theme.displayFamily
            font.weight: Font.Bold
            font.pixelSize: Math.max(12, root.size * 0.4)
        }

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: 1
            border.color: root.online ? "#2f6b55" : "#3b2430"
        }
    }

    Rectangle {
        visible: root.online
        width: Math.max(7, root.size * 0.18)
        height: width
        radius: width / 2
        color: theme.green
        border.width: 2
        border.color: theme.background
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
}
