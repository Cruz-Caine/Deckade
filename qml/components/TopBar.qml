import QtQuick

Item {
    id: root

    property string title: "HOME"
    property string subtitle: ""
    property var user: ({})

    height: 72

    Column {
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        width: Math.max(240, parent.width - 610)

        Text {
            width: parent.width
            elide: Text.ElideRight
            text: root.title.toUpperCase()
            color: theme.text
            font.family: theme.displayFamily
            font.weight: Font.Bold
            font.pixelSize: 26
            font.letterSpacing: 2.4
        }

        Text {
            width: parent.width
            elide: Text.ElideRight
            text: root.subtitle
            color: theme.textFaint
            font.family: theme.bodyFamily
            font.pixelSize: 12
        }
    }

    Row {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        Row {
            spacing: 6
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                width: 7
                height: 7
                radius: 4
                color: client.connected ? theme.green
                     : client.status === "connecting" || client.status === "authenticating" ? theme.gold
                     : theme.red
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: client.connected ? "ONLINE" : client.status.toUpperCase()
                color: client.connected ? theme.green : theme.textFaint
                font.family: theme.displayFamily
                font.weight: Font.DemiBold
                font.pixelSize: 10
                font.letterSpacing: 1.4
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Rectangle {
            width: 1
            height: 24
            color: theme.border
            anchors.verticalCenter: parent.verticalCenter
        }

        Pill {
            text: launcher.runtimeSummary.some(function(r) { return r.available }) ? "NATIVE RUNTIME" : "RUNTIME MISSING"
            strong: true
            foreground: launcher.runtimeSummary.some(function(r) { return r.available }) ? theme.cyan : theme.red
            line: launcher.runtimeSummary.some(function(r) { return r.available }) ? "#24516b" : "#5a2430"
            fill: "#0e1620"
            anchors.verticalCenter: parent.verticalCenter
        }

        Row {
            spacing: 9
            anchors.verticalCenter: parent.verticalCenter

            Avatar {
                size: 36
                hash: root.user.gravatar ?? ""
                name: root.user.displayName ?? root.user.name ?? ""
                online: true
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0

                Text {
                    text: root.user.displayName ?? root.user.name ?? ""
                    color: theme.text
                    font.family: theme.displayFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 15
                }
                Text {
                    text: root.user.name ?? ""
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 10
                }
            }
        }
    }
}
