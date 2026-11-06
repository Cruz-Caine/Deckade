import QtQuick

Focusable {
    id: root

    property var channel: ({})
    property bool showSystem: true
    property bool joined: false
    property bool favorite: false

    signal joinRequested()
    signal favoriteRequested()

    ringRadius: 14
    onActivated: root.joinRequested()
    onSecondaryActivated: root.favoriteRequested()

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: theme.panel
        border.width: 1
        border.color: root.activeFocus ? theme.accent : theme.border
        clip: true

        Image {
            id: preview
            anchors.fill: parent
            source: root.channel.previewUrl ?? ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            smooth: true
            opacity: 0.92
        }

        Rectangle {
            anchors.fill: parent
            visible: preview.status !== Image.Ready
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#253248" }
                GradientStop { position: 1.0; color: "#10151f" }
            }
            Text {
                anchors.centerIn: parent
                text: (root.channel.title ?? "?").slice(0, 2).toUpperCase()
                color: theme.textFaint
                font.family: theme.displayFamily
                font.pixelSize: 34
                font.weight: Font.Bold
            }
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#080b11" }
                GradientStop { position: 0.45; color: "#080b1100" }
                GradientStop { position: 1.0; color: "#080b11ee" }
            }
        }

        Row {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 7

            Pill {
                text: String(root.channel.playerCount ?? 0)
                dot: true
                foreground: theme.green
                fill: "#0b1512dd"
                line: "#245544"
            }
            Pill {
                visible: root.channel.isRanked === true
                text: "RANKED"
                strong: true
                foreground: theme.gold
                fill: "#1a160bdd"
                line: "#5b4720"
            }
            Pill {
                visible: root.favorite
                text: "★"
                foreground: theme.gold
                fill: "#1a160bdd"
                line: "#5b4720"
            }
        }

        Rectangle {
            visible: root.joined
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            width: joinedLabel.implicitWidth + 18
            height: 26
            radius: 8
            color: "#0d1a2bdd"
            border.width: 1
            border.color: theme.cyan
            Text {
                id: joinedLabel
                anchors.centerIn: parent
                text: "JOINED"
                color: theme.cyan
                font.family: theme.displayFamily
                font.weight: Font.DemiBold
                font.pixelSize: 11
                font.letterSpacing: 1.2
            }
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 14
            spacing: 3

            Text {
                width: parent.width
                text: root.channel.title ?? root.channel.name ?? ""
                color: theme.text
                elide: Text.ElideRight
                font.family: theme.displayFamily
                font.weight: Font.Bold
                font.pixelSize: 20
                font.letterSpacing: 0.3
            }
            Text {
                visible: root.showSystem
                width: parent.width
                text: [root.channel.system, root.channel.gameId].filter(Boolean).join("  ·  ")
                color: theme.textMuted
                elide: Text.ElideRight
                font.family: theme.bodyFamily
                font.pixelSize: 12
            }
        }
    }

    scale: root.activeFocus ? 1.025 : 1.0
    Behavior on scale {
        NumberAnimation { duration: settings.reduceMotion ? 0 : 120; easing.type: Easing.OutCubic }
    }
}
