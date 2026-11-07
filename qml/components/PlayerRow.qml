import QtQuick
import QtQuick.Controls

Focusable {
    id: root

    property var player: ({})
    property bool isLocal: false
    property bool isFriend: false

    signal challengeRequested(var player)
    signal copyRequested(string name)
    signal friendToggleRequested(var player)

    implicitHeight: 58
    ringRadius: 9
    onActivated: root.challengeRequested(root.player)
    onSecondaryActivated: root.friendToggleRequested(root.player)

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
        if (rank <= 0) return ""
        if (rank === 1) return "E"
        if (rank === 2) return "D"
        if (rank === 3) return "C"
        if (rank === 4) return "B"
        if (rank === 5) return "A"
        if (rank >= 6) return "S"
        return String(rank)
    }



    function statusText() {
        if (root.player.playing && root.player.playing.quarkId) return "PLAYING"
        if (root.player.away || root.player.channelAway) return "AWAY"
        return "READY"
    }

    Rectangle {
        anchors.fill: parent
        radius: 9
        color: root.activeFocus ? theme.panelHover : "transparent"
        border.width: root.activeFocus ? 1 : 0
        border.color: theme.accent
        Behavior on color { ColorAnimation { duration: settings.reduceMotion ? 0 : 90 } }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 8
        spacing: 8

        Image {
            width: 22
            height: 15
            anchors.verticalCenter: parent.verticalCenter
            source: "qrc:/assets/flags/" + ((root.player.country?.code ?? "unknown").toLowerCase()) + ".png"
            fillMode: Image.Stretch
            asynchronous: true
            cache: true
        }

        Column {
            width: Math.max(70, parent.width - 142)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
                width: parent.width
                text: root.player.name ?? ""
                color: root.isLocal ? theme.gold
                     : (root.player.playing?.quarkId ? theme.textMuted : theme.text)
                font.family: theme.displayFamily
                font.weight: Font.DemiBold
                font.pixelSize: 16
                elide: Text.ElideRight
            }

            Row {
                spacing: 7
                Text {
                    text: root.statusText()
                    color: root.statusText() === "READY" ? theme.green
                         : root.statusText() === "PLAYING" ? theme.cyan : theme.textFaint
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 9
                    font.letterSpacing: 0.8
                }
                Text {
                    visible: (root.player.matchCount ?? 0) > 0
                    text: (root.player.matchCount ?? 0) + " MATCHES"
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 9
                }
                Text {
                    visible: root.isLocal
                    text: "YOU"
                    color: theme.cyan
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 9
                }
                Text {
                    visible: root.isFriend
                    text: "★ FRIEND"
                    color: theme.gold
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 9
                }
            }
        }

        Item { width: 1; height: 1 }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            Repeater {
                model: 4
                Rectangle {
                    width: 4
                    height: 8 + index * 3
                    radius: 2
                    color: index < root.pingQuality() ? theme.green : theme.border
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Item {
            width: 24
            height: 20
            anchors.verticalCenter: parent.verticalCenter
            Image {
                visible: (root.player.rank ?? 0) > 0
                anchors.fill: parent
                source: "qrc:/assets/ranks/rank" + (root.player.rank ?? 0) + ".png"
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: true
            }
            Text {
                visible: !((root.player.rank ?? 0) > 0)
                anchors.centerIn: parent
                text: "—"
                color: theme.textFaint
                font.family: theme.bodyFamily
                font.pixelSize: 10
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.player.playing?.quarkId ? "▶" : "＋"
            color: root.player.playing?.quarkId ? theme.cyan : theme.textFaint
            font.family: theme.bodyFamily
            font.pixelSize: 15
            width: 16
            horizontalAlignment: Text.AlignRight
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: theme.border
        opacity: 0.35
    }
}
