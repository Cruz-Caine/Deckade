import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: screen

    property var channel: ({})
    property var messages: []
    property var user: ({})
    property var incomingChallenges: []
    property var outgoingChallenges: []
    property bool launching: false
    property bool favorite: false
    property string playerSearch: ""
    property var visibleUsers: []
    property var friends: []
    property bool friendsOnly: false
    property string focusedPlayerName: ""
    property string channelKey: ""
    property bool preservingPlayerFocus: false
    property bool autoMatchActive: false

    signal sendChat(string text)
    signal challengePlayer(var player)
    signal spectateRequested(var match)
    signal leaveRequested()
    signal testRequested()
    signal trainingRequested()
    signal favoriteRequested(bool favorite)
    signal controlsRequested()
    signal acceptChallenge(var challenge)
    signal declineChallenge(var challenge)
    signal cancelChallenge(var challenge)
    signal copyRequested(string text)
    signal friendToggleRequested(var player)
    signal autoMatchToggleRequested()
    signal channelTvRequested()

    // Keep an outgoing challenge visible until the server reports an accept,
    // reject, cancel, or match start. A transient toast alone leaves the player
    // unsure whether the challenge was actually delivered.
    Rectangle {
        id: outgoingBanner
        z: 30
        visible: screen.outgoingChallenges.length > 0
        anchors.top: parent.top
        anchors.topMargin: 154
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(520, parent.width - 40)
        height: 64
        radius: 12
        color: "#16130bdd"
        border.width: 1
        border.color: theme.gold

        Row {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 10
            spacing: 12

            Column {
                width: parent.width - 126
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    width: parent.width
                    text: "CHALLENGE SENT · " + (screen.outgoingChallenges.length > 0 ? (screen.outgoingChallenges[0].challenger ?? "") : "")
                    color: theme.gold
                    font.family: theme.displayFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: screen.outgoingChallenges.length > 1
                          ? "Waiting for a response · " + screen.outgoingChallenges.length + " pending"
                          : "Waiting for a response"
                    color: theme.textMuted
                    font.family: theme.bodyFamily
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }

            AppButton {
                text: "CANCEL"
                compact: true
                danger: true
                width: 108
                height: 42
                anchors.verticalCenter: parent.verticalCenter
                onClicked: screen.cancelChallenge(screen.outgoingChallenges[0])
            }
        }
    }

    function isLocal(player) {
        return (screen.user.name ?? "") === (player.name ?? "")
    }

    function sortedUsers() {
        var users = (screen.channel.users ?? []).slice()
        users.sort(function(a, b) {
            // Put available players first: the room should open on someone you
            // can actually challenge, not a wall of in-progress matches.
            var aPlaying = a.playing && a.playing.quarkId ? 1 : 0
            var bPlaying = b.playing && b.playing.quarkId ? 1 : 0
            if (screen.isLocal(a)) return -1
            if (screen.isLocal(b)) return 1
            if (aPlaying !== bPlaying) return aPlaying - bPlaying
            var aAway = a.away || a.channelAway
            var bAway = b.away || b.channelAway
            if (aAway !== bAway) return aAway ? 1 : -1
            return (a.name ?? "").localeCompare(b.name ?? "")
        })
        return users
    }

    function refreshVisibleUsers() {
        const active = screen.Window.window ? screen.Window.window.activeFocusItem : null
        if (active && active.player && active.player.name) {
            focusedPlayerName = active.player.name
        } else if (active) {
            focusedPlayerName = ""
        }
        const query = playerSearch.trim().toLowerCase()
        var users = sortedUsers()
        if (friendsOnly) {
            users = users.filter(function(player) {
                return screen.friends.some(function(name) {
                    return name.toLowerCase() === (player.name ?? "").toLowerCase()
                })
            })
        }
        if (query.length > 0) {
            users = users.filter(function(player) {
                return (player.name ?? "").toLowerCase().indexOf(query) !== -1
            })
        }
        preservingPlayerFocus = true
        visibleUsers = users
        Qt.callLater(function() { screen.restorePlayerFocus() })
    }

    function restorePlayerFocus() {
        if (!focusedPlayerName) {
            preservingPlayerFocus = false
            return
        }
        var index = visibleUsers.findIndex(function(player) {
            return (player.name ?? "") === focusedPlayerName
        })
        if (index >= 0) {
            playerList.positionViewAtIndex(index, ListView.Contain)
            playerList.currentIndex = index
            Qt.callLater(function() {
                var item = playerList.itemAtIndex(index)
                if (item) focusController.focusItem(item)
                preservingPlayerFocus = false
            })
        } else {
            preservingPlayerFocus = false
        }
    }

    onFriendsChanged: refreshVisibleUsers()
    onFriendsOnlyChanged: refreshVisibleUsers()

    onChannelChanged: {
        const nextKey = channel.name ?? ""
        if (nextKey !== channelKey) {
            channelKey = nextKey
            playerSearch = ""
            focusedPlayerName = ""
        }
        refreshVisibleUsers()
    }
    onPlayerSearchChanged: {
        focusedPlayerName = ""
        refreshVisibleUsers()
        playerList.positionViewAtBeginning()
    }
    Component.onCompleted: refreshVisibleUsers()

    Connections {
        target: focusController
        function onFocusMoved(item) {
            if (screen.preservingPlayerFocus || !item) return
            if (item.player && item.player.name) screen.focusedPlayerName = item.player.name
            else screen.focusedPlayerName = ""
        }
    }

    function liveMatches() {
        return (screen.channel.quarks ?? []).slice(0, 5)
    }

    function liveMatchCount() {
        return (screen.channel.quarks ?? []).length
    }

    function matchPlayerNames(quark) {
        var names = quark.players ?? []
        if (names.length > 0) return names.join("  vs  ")
        return "Live match"
    }

    function sendMessage() {
        var text = chatInput.text.trim()
        if (text.length === 0) return
        chatInput.text = ""
        screen.sendChat(text)
    }

    function focusDefault() {
        Qt.callLater(function() {
            if (playerList.count > 0 && playerList.itemAtIndex(0)) {
                focusController.focusItem(playerList.itemAtIndex(0))
            } else if (chatInput) {
                chatInput.forceActiveFocus(Qt.OtherFocusReason)
                focusController.focusItem(chatInput)
            }
        })
    }

    Rectangle {
        anchors.fill: parent
        radius: 16
        color: "#0f141e"
        border.width: 1
        border.color: theme.border
        clip: true

        Image {
            width: parent.width
            height: 188
            source: screen.channel.previewUrl ?? ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            opacity: 0.48
        }

        Rectangle {
            width: parent.width
            height: 188
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#0f141ecc" }
                GradientStop { position: 0.55; color: "#0f141e77" }
                GradientStop { position: 1.0; color: "#0f141e" }
            }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Row {
                width: parent.width
                height: 132

                Column {
                    width: parent.width - 624
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 5

                    Row {
                        spacing: 8
                        width: parent.width
                        Text {
                            text: "#" + (screen.channel.name ?? "")
                            color: theme.text
                            font.family: theme.displayFamily
                            font.weight: Font.Bold
                            font.pixelSize: 30
                            elide: Text.ElideRight
                            width: parent.width - 78
                        }
                        Pill {
                            text: "RANKED"
                            visible: screen.channel.isRanked === true
                            strong: true
                            foreground: theme.gold
                            line: "#5b4720"
                            fill: "#1a160b"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Text {
                        text: [screen.channel.title, screen.channel.system, screen.channel.gameId].filter(Boolean).join("   ·   ")
                        color: theme.textMuted
                        font.family: theme.bodyFamily
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        width: parent.width
                    }

                    Row {
                        spacing: 14
                        Pill {
                            text: String(screen.visibleUsers.filter(function(u) { return !u.away && !u.channelAway }).length) + " ONLINE"
                            dot: true
                            foreground: theme.green
                            fill: "#0b1512"
                            line: "#245544"
                        }
                        Pill {
                            text: String(screen.incomingChallenges.length) + " CHALLENGES"
                            visible: screen.incomingChallenges.length > 0
                            foreground: theme.gold
                            fill: "#1a160b"
                            line: "#5b4720"
                        }
                    }
                }

                Row {
                    width: 610
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter
                    layoutDirection: Qt.RightToLeft

                    AppButton {
                        text: "LEAVE"
                        compact: true
                        danger: true
                        width: 92
                        onClicked: screen.leaveRequested()
                    }
                    AppButton {
                        text: screen.favorite ? "★ FAVORITE" : "☆ FAVORITE"
                        compact: true
                        width: 130
                        onClicked: screen.favoriteRequested(!screen.favorite)
                    }
                    AppButton {
                        visible: (screen.channel.emulator ?? "").toLowerCase() === "fbneo"
                        text: "CONTROLS"
                        compact: true
                        width: 118
                        onClicked: screen.controlsRequested()
                    }
                    AppButton {
                        visible: screen.channel.supportsTraining === true
                        text: "TRAINING"
                        compact: true
                        width: 112
                        onClicked: screen.trainingRequested()
                    }
                    AppButton {
                        text: "TEST GAME"
                        compact: true
                        width: 120
                        onClicked: screen.testRequested()
                    }
                }
            }

            Row {
                width: parent.width
                height: parent.height - 146
                spacing: 14

                Rectangle {
                    width: Math.max(250, Math.min(330, parent.width * 0.30))
                    height: parent.height
                    radius: 13
                    color: "#0c111acc"
                    border.width: 1
                    border.color: theme.border

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Row {
                            width: parent.width
                            height: 22
                            Text {
                                text: "PLAYERS"
                                color: theme.textMuted
                                font.family: theme.displayFamily
                                font.weight: Font.DemiBold
                                font.pixelSize: 12
                                font.letterSpacing: 1.5
                            }
                            Item { width: parent.width - 110; height: 1 }
                            Text {
                                text: screen.playerSearch.length > 0
                                      ? screen.visibleUsers.length + " / " + screen.sortedUsers().length
                                      : String(screen.sortedUsers().length) + " IN ROOM"
                                color: theme.textFaint
                                font.family: theme.bodyFamily
                                font.pixelSize: 10
                            }
                        }

                        AppButton {
                            width: parent.width
                            height: 34
                            compact: true
                            text: screen.friendsOnly ? "SHOWING FRIENDS" : "FILTER FRIENDS"
                            selected: screen.friendsOnly
                            onClicked: screen.friendsOnly = !screen.friendsOnly
                        }

                        AppButton {
                            width: parent.width
                            height: 34
                            compact: true
                            primary: screen.autoMatchActive
                            text: screen.autoMatchActive ? "AUTO MATCHING..." : "AUTO MATCH"
                            onClicked: screen.autoMatchToggleRequested()
                        }

                        DeckField {
                            id: playerSearchField
                            width: parent.width
                            height: 36
                            font.pixelSize: 14
                            placeholderText: "Search players"
                            text: screen.playerSearch
                            onTextChanged: screen.playerSearch = text
                        }

                        ListView {
                            id: playerList
                            width: parent.width
                            height: parent.height - 148
                            clip: true
                            spacing: 1
                            model: screen.visibleUsers
                            boundsBehavior: Flickable.StopAtBounds
                            property real stableY: 0

                            onMovementEnded: stableY = contentY
                            onModelChanged: {
                                const target = stableY
                                Qt.callLater(function() {
                                    contentY = Math.min(target, Math.max(0, contentHeight - height))
                                    screen.restorePlayerFocus()
                                })
                            }

                            ScrollBar.vertical: ScrollBar {
                                id: playerScrollBar
                                policy: ScrollBar.AlwaysOn
                                width: 10
                                contentItem: Rectangle {
                                    implicitWidth: 6
                                    radius: 3
                                    color: playerScrollBar.pressed ? theme.accent
                                         : playerScrollBar.hovered ? theme.borderStrong : theme.border
                                }
                                background: Rectangle {
                                    implicitWidth: 6
                                    radius: 3
                                    color: "#0a0f16"
                                }
                            }

                            delegate: PlayerRow {
                                width: ListView.view.width
                                player: modelData
                                isLocal: screen.isLocal(modelData)
                                isFriend: screen.friends.some(function(name) { return name.toLowerCase() === (modelData.name ?? "").toLowerCase() })
                                onChallengeRequested: screen.challengePlayer(modelData)
                                onFriendToggleRequested: screen.friendToggleRequested(modelData)
                                onCopyRequested: screen.copyRequested(modelData.name ?? "")
                            }
                        }

                        // Always-visible custom scrollbar. The attached Qt
                        // ScrollBar was being clipped by the ListView viewport.
                        Rectangle {
                            visible: playerList.visibleArea.heightRatio < 0.999
                            width: 8
                            height: playerList.height
                            radius: 4
                            color: "#182130"
                            x: playerList.x + playerList.width - width
                            y: playerList.y
                            z: 5

                            Rectangle {
                                width: parent.width
                                radius: 3
                                color: playerList.dragging ? theme.accent : theme.cyan
                                height: Math.max(24, playerList.visibleArea.heightRatio * parent.height)
                                y: Math.max(0, playerList.visibleArea.yPosition * parent.height)
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                            - Math.max(250, Math.min(330, parent.width * 0.30))
                            - Math.max(230, Math.min(300, parent.width * 0.27)) - 28
                    height: parent.height
                    radius: 13
                    color: "#0c111acc"
                    border.width: 1
                    border.color: theme.border

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Row {
                            width: parent.width
                            height: 22
                            Text {
                                text: "CHAT"
                                color: theme.textMuted
                                font.family: theme.displayFamily
                                font.weight: Font.DemiBold
                                font.pixelSize: 12
                                font.letterSpacing: 1.5
                            }
                            Item { width: parent.width - 100; height: 1 }
                            Text {
                                text: "LIVE"
                                color: theme.green
                                font.family: theme.bodyFamily
                                font.weight: Font.DemiBold
                                font.pixelSize: 10
                                font.letterSpacing: 1
                            }
                        }

                        ListView {
                            id: chatList
                            width: parent.width
                            height: parent.height - 86
                            clip: true
                            spacing: 6
                            model: screen.messages
                            boundsBehavior: Flickable.StopAtBounds

                            onCountChanged: positionViewAtEnd()

                            delegate: Item {
                                id: messageRow
                                required property var modelData
                                width: chatList.width
                                height: messageText.implicitHeight + 8

                                Text {
                                    id: messageText
                                    width: parent.width
                                    textFormat: Text.PlainText
                                    text: (modelData.kind === "system")
                                          ? "— " + (modelData.text ?? "")
                                          : (modelData.username ?? "") + "   " + (modelData.text ?? "")
                                    color: modelData.kind === "system" ? theme.textFaint
                                         : modelData.kind === "error" ? theme.red
                                         : theme.text
                                    font.family: theme.bodyFamily
                                    font.pixelSize: 13
                                    lineHeight: 1.25
                                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                }
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: 8

                            DeckField {
                                id: chatInput
                                width: parent.width - 104
                                height: 46
                                placeholderText: "Message #" + (screen.channel.name ?? "")
                                onAccepted: screen.sendMessage()
                            }

                            AppButton {
                                text: "SEND"
                                compact: true
                                width: 96
                                height: 46
                                onClicked: screen.sendMessage()
                            }
                        }
                    }
                }

                Rectangle {
                    width: Math.max(230, Math.min(300, parent.width * 0.27))
                    height: parent.height
                    radius: 13
                    color: "#0c111acc"
                    border.width: 1
                    border.color: theme.border

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        Text {
                            text: "LIVE MATCHES"
                            color: theme.textMuted
                            font.family: theme.displayFamily
                            font.weight: Font.DemiBold
                            font.pixelSize: 12
                            font.letterSpacing: 1.5
                        }

                        AppButton {
                            width: parent.width
                            height: 30
                            compact: true
                            text: "WATCH TOP MATCH"
                            onClicked: screen.channelTvRequested()
                        }

                        Text {
                            visible: screen.liveMatches().length === 0
                            width: parent.width
                            text: "No live matches yet. Challenge someone or start a test game."
                            color: theme.textFaint
                            font.family: theme.bodyFamily
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            visible: screen.liveMatchCount() > 5
                            width: parent.width
                            text: "+" + (screen.liveMatchCount() - 5) + " more live matches"
                            color: theme.cyan
                            font.family: theme.bodyFamily
                            font.pixelSize: 10
                        }

                        Repeater {
                            model: screen.liveMatches()

                            delegate: Focusable {
                                id: matchItem
                                required property var modelData
                                width: parent.width
                                height: 58
                                ringRadius: 9
                                onActivated: screen.spectateRequested(modelData)

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 9
                                    color: matchItem.activeFocus ? theme.panelHover : theme.panel
                                    border.width: 1
                                    border.color: matchItem.activeFocus ? theme.accent : theme.border
                                }

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 9
                                    spacing: 2
                                    Text {
                                        width: parent.width
                                        text: screen.matchPlayerNames(modelData)
                                        color: theme.text
                                        elide: Text.ElideRight
                                        font.family: theme.displayFamily
                                        font.weight: Font.DemiBold
                                        font.pixelSize: 14
                                    }
                                    Text {
                                        text: (modelData.ranked > 0 ? "FT" + modelData.ranked + "  ·  " : "CASUAL  ·  ")
                                              + modelData.spectators + " watching"
                                        color: theme.textFaint
                                        font.family: theme.bodyFamily
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        Item { width: 1; height: 2 }

                        Text {
                            text: "CHANNEL NOTES"
                            visible: (screen.channel.motd ?? "").length > 0
                            color: theme.textMuted
                            font.family: theme.displayFamily
                            font.weight: Font.DemiBold
                            font.pixelSize: 12
                            font.letterSpacing: 1.5
                        }

                        Text {
                            width: parent.width
                            text: screen.channel.motd ?? ""
                            visible: text.length > 0
                            color: theme.textFaint
                            font.family: theme.bodyFamily
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                            maximumLineCount: 5
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

}
