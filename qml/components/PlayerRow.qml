import QtQuick

Focusable {
    id: root

    property var player: ({})
    property bool isLocal: false

    signal challengeRequested(var player)
    signal copyRequested(string name)

    implicitHeight: 64
    ringRadius: 10
    onActivated: root.challengeRequested(root.player)
    onSecondaryActivated: root.copyRequested(root.player.name ?? "")

    function pingQuality() {
        if (root.player.isWifi) return 2
        var ping = root.player.virtualPing >= 0 ? root.player.virtualPing : (root.player.ping >= 0 ? root.player.ping : 200)
        if (ping < 55) return 4
        if (ping < 95) return 3
        if (ping < 150) return 2
        return 1
    }

    function rankText() {
        var rank = root.player.rank ?? 0
        if (rank <= 0) return "—"
        if (rank === 1) return "E"
        if (rank === 2) return "D"
        if (rank === 3) return "C"
        if (rank === 4) return "B"
        if (rank === 5) return "A"
        return String(rank)
    }

    function statusText() {
        if (root.player.playing && root.player.playing.quarkId) return "PLAYING"
        if (root.player.away) return "AWAY"
        return "READY"
    }

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: root.activeFocus ? theme.panelHover : "transparent"
        Behavior on color { ColorAnimation { duration: settings.reduceMotion ? 0 : 90 } }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 10
        spacing: 10

        Avatar {
            size: 40
            hash: root.player.gravatar ?? ""
            name: root.player.name ?? ""
            online: !(root.player.away === true)
            anchors.verticalCenter: parent.verticalCenter
        }

        Column {
            width: Math.max(100, parent.width - 160)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: root.player.name ?? ""
                color: theme.text
                font.family: theme.displayFamily
                font.weight: Font.DemiBold
                font.pixelSize: 17
                elide: Text.ElideRight
            }

            Row {
                spacing: 7
                Text {
                    text: "RANK " + root.rankText()
                    color: theme.gold
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 10
                    font.letterSpacing: 0.8
                }
                Text {
                    text: root.statusText()
                    color: root.statusText() === "READY" ? theme.green
                         : root.statusText() === "PLAYING" ? theme.cyan : theme.textFaint
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 10
                    font.letterSpacing: 0.8
                }
                Text {
                    visible: root.isLocal
                    text: "YOU"
                    color: theme.cyan
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 10
                    font.letterSpacing: 0.8
                }
                Text {
                    visible: (root.player.country?.code ?? "").length > 0
                    text: (root.player.country?.code ?? "").toUpperCase()
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 10
                }
            }
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Repeater {
                model: 4
                Rectangle {
                    width: 5
                    height: 9 + index * 4
                    radius: 2
                    color: index < root.pingQuality() ? theme.green : theme.border
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.player.playing?.quarkId ? "▶" : root.player.away ? "◌" : "＋"
            color: root.player.playing?.quarkId ? theme.cyan : theme.textFaint
            font.family: theme.bodyFamily
            font.pixelSize: 18
            width: 20
            horizontalAlignment: Text.AlignRight
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: theme.border
        opacity: 0.45
    }
}
