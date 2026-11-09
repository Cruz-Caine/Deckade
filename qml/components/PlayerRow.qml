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

    implicitHeight: 48
    ringRadius: 6
    ringOutside: false
    hintPrimary: "Challenge"
    hintSecondary: root.isFriend ? "Remove friend" : "Add friend"
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

    function nameColor() {
        if (root.player.playing && root.player.playing.quarkId) return theme.textMuted
        if (root.player.away || root.player.channelAway) return theme.textFaint
        return theme.text
    }

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: root.activeFocus ? theme.panelHover : "transparent"
        Behavior on color { ColorAnimation { duration: settings.reduceMotion ? 0 : 80 } }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 8
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

        Text {
            // Flag, ping bars, rank badge, one spacer and five gaps sit to the
            // right of the name; leave room for all of them so the badge is
            // never pushed out of the row.
            width: Math.max(60, parent.width - 120)
            text: (root.isFriend ? "★ " : "") + (root.player.name ?? "")
            color: root.nameColor()
            font.family: theme.displayFamily
            font.weight: Font.DemiBold
            font.pixelSize: 15
            elide: Text.ElideRight
            anchors.verticalCenter: parent.verticalCenter
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
            width: 22
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
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: theme.border
        opacity: 0.0
    }
}
