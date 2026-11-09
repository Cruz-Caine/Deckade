import QtQuick

Focusable {
    id: root

    property var channel: ({})
    property bool showSystem: true
    property bool joined: false
    property bool favorite: false
    // Re-evaluated whenever a download finishes.
    readonly property var files: {
        roms.revision
        return root.channel.emulator && root.channel.gameId
            ? roms.fileStatus(root.channel.emulator, root.channel.gameId)
            : ({ known: false })
    }

    signal joinRequested()
    signal joinToggleRequested()
    signal favoriteRequested()

    ringRadius: 8
    hintPrimary: joined ? "Open room" : "Join and open"
    hintSecondary: joined ? "Leave room" : "Join"
    hintTertiary: favorite ? "Unfavorite" : "Favorite"
    onActivated: root.joinRequested()
    onSecondaryActivated: root.joinToggleRequested()
    onTertiaryActivated: root.favoriteRequested()

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: theme.panel
        clip: true

        Image {
            id: preview
            anchors.fill: parent
            anchors.bottomMargin: 62
            source: root.channel.previewUrl ?? ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            smooth: true
        }

        Rectangle {
            anchors.fill: preview
            visible: preview.status !== Image.Ready
            color: theme.panelRaised
            Text {
                anchors.centerIn: parent
                text: (root.channel.title ?? "?").slice(0, 2)
                color: theme.textFaint
                font.family: theme.bodyFamily
                font.pixelSize: 26
                font.weight: Font.DemiBold
            }
        }

        Row {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 8
            spacing: 5

            Pill {
                text: String(root.channel.playerCount ?? 0) + " online"
                dot: true
                foreground: theme.green
            }
            Pill {
                visible: root.joined
                text: "Joined"
                foreground: theme.cyan
            }
            Pill {
                visible: root.favorite
                text: "★"
                foreground: theme.gold
            }
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.bottomMargin: 9
            spacing: 3

            Text {
                width: parent.width
                text: root.channel.title ?? root.channel.name ?? ""
                color: theme.text
                elide: Text.ElideRight
                font.family: theme.bodyFamily
                font.weight: Font.DemiBold
                font.pixelSize: 14
            }

            Row {
                width: parent.width
                spacing: 8

                Text {
                    visible: root.showSystem
                    width: parent.width - fileLabel.width - 8
                    text: (root.channel.isRanked === true ? "Ranked, " : "") + (root.channel.system ?? "")
                    color: theme.textFaint
                    elide: Text.ElideRight
                    font.family: theme.bodyFamily
                    font.pixelSize: 11
                }

                Text {
                    id: fileLabel
                    visible: root.files.known === true
                    text: root.files.ready ? "Downloaded" : "Not downloaded"
                    color: root.files.ready ? theme.green : theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 11
                }
            }
        }
    }
}
