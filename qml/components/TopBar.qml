import QtQuick

Item {
    id: root

    property string title: "Home"
    property string subtitle: ""
    property var user: ({})

    height: 52

    Column {
        anchors.left: parent.left
        anchors.right: account.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1

        Text {
            width: parent.width
            elide: Text.ElideRight
            text: root.title
            color: theme.text
            font.family: theme.bodyFamily
            font.weight: Font.Bold
            font.pixelSize: 22
        }

        Text {
            visible: root.subtitle.length > 0
            width: parent.width
            elide: Text.ElideRight
            text: root.subtitle
            color: theme.textFaint
            font.family: theme.bodyFamily
            font.pixelSize: 12
        }
    }

    Row {
        id: account
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.user.displayName ?? root.user.name ?? ""
            color: theme.textMuted
            font.family: theme.bodyFamily
            font.weight: Font.Medium
            font.pixelSize: 13
        }

        Avatar {
            size: 32
            hash: root.user.gravatar ?? ""
            name: root.user.displayName ?? root.user.name ?? ""
            online: client.connected
        }
    }
}
