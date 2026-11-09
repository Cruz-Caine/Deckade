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
    // Roster and chat are ListModels updated row by row. Assigning a fresh
    // array to a ListView resets it: every row is destroyed, the selection is
    // lost and the list jumps to the top.
    property var playerLookup: Object.create(null)
    property var playerKeys: []
    property var shownMessages: []
    property bool followChat: false
    property var liveMatchKeys: []
    property bool autoMatchActive: false

    signal sendChat(string text)
    signal challengePlayer(var player)
    signal spectateRequested(var match)
    signal leaveRequested()
    signal testRequested()
    signal trainingRequested()
    signal favoriteRequested(bool favorite)
    signal acceptChallenge(var challenge)
    signal declineChallenge(var challenge)
    signal cancelChallenge(var challenge)
    signal copyRequested(string text)
    signal friendToggleRequested(var player)
    signal autoMatchToggleRequested()
    signal channelTvRequested()
    signal controlsRequested()
    signal filesRequested()

    readonly property var files: {
        roms.revision
        return channel.emulator && channel.gameId
            ? roms.fileStatus(channel.emulator, channel.gameId)
            : ({ known: false })
    }

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
        radius: 10
        color: theme.panelRaised
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
                text: "Cancel"
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
        var lookup = Object.create(null)
        users.forEach(function(player) { lookup[player.name ?? ""] = player })
        playerLookup = lookup
        visibleUsers = users
        syncPlayerModel(users.map(function(player) { return player.name ?? "" }))
    }

    function syncPlayerModel(names) {
        var current = playerKeys
        var focusedIndex = focusedPlayerName ? current.indexOf(focusedPlayerName) : -1
        var focusedGone = focusedIndex >= 0 && names.indexOf(focusedPlayerName) < 0
        var wanted = new Set(names)
        for (var r = current.length - 1; r >= 0; --r) {
            if (!wanted.has(current[r])) {
                playerModel.remove(r)
                current.splice(r, 1)
            }
        }
        for (var i = 0; i < names.length; ++i) {
            if (current[i] === names[i]) continue
            var from = current.indexOf(names[i], i + 1)
            if (from >= 0) {
                playerModel.move(from, i, 1)
                current.splice(from, 1)
            } else {
                playerModel.insert(i, { playerKey: names[i] })
            }
            current.splice(i, 0, names[i])
        }
        while (current.length > names.length) {
            playerModel.remove(current.length - 1)
            current.pop()
        }
        if (focusedGone) focusPlayerNear(focusedIndex)
    }

    // The selected player left: stay at the same spot in the list.
    function focusPlayerNear(index) {
        focusedPlayerName = ""
        Qt.callLater(function() {
            var target = Math.min(index, playerList.count - 1)
            if (target < 0 || !screen.visible) return
            playerList.positionViewAtIndex(target, ListView.Contain)
            var item = playerList.itemAtIndex(target)
            if (item) focusController.focusItem(item)
        })
    }

    function chatNearEnd() {
        return chatList.contentY + chatList.height >= chatList.originY + chatList.contentHeight - 48
    }

    function chatRow(message) {
        return {
            kind: String(message.kind ?? ""),
            username: String(message.username ?? ""),
            body: String(message.text ?? "")
        }
    }

    function syncChat() {
        var next = screen.messages ?? []
        var previous = shownMessages
        var follow = followChat || chatModel.count === 0 || chatNearEnd()
        // Normally the new list is the old one plus a message, minus the
        // oldest once the 400-message cap is reached.
        var dropped = next.length > 0 ? previous.indexOf(next[0]) : -1
        var continues = dropped >= 0 && previous.length - dropped <= next.length
        for (var k = dropped; continues && k < previous.length; ++k) {
            if (previous[k] !== next[k - dropped]) continues = false
        }
        var firstNew = 0
        if (continues) {
            if (dropped > 0) chatModel.remove(0, dropped)
            firstNew = previous.length - dropped
        } else {
            chatModel.clear()
            follow = true
        }
        if (firstNew < next.length) chatModel.append(next.slice(firstNew).map(chatRow))
        shownMessages = next
        followChat = false
        if (follow) chatList.positionViewAtEnd()
    }

    onMessagesChanged: syncChat()

    function matchKey(quark) {
        return String(quark.quarkId ?? "")
    }

    function matchByKey(key) {
        return (screen.channel.quarks ?? []).find(function(quark) { return matchKey(quark) === key }) ?? ({})
    }

    function refreshLiveMatchKeys() {
        var next = liveMatches().map(matchKey)
        if (next.join("\n") !== liveMatchKeys.join("\n")) liveMatchKeys = next
    }

    onFriendsChanged: refreshVisibleUsers()
    onFriendsOnlyChanged: refreshVisibleUsers()

    onChannelChanged: {
        const nextKey = channel.name ?? ""
        if (nextKey !== channelKey) {
            channelKey = nextKey
            playerSearch = ""
            focusedPlayerName = ""
            playerModel.clear()
            playerKeys = []
            playerList.positionViewAtBeginning()
        }
        refreshVisibleUsers()
        refreshLiveMatchKeys()
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
            if (!item) return
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
        // Your own message always brings the log back to the bottom.
        followChat = true
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
        radius: 10
        color: "#1b1f29"
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
            opacity: 0.35
        }

        Rectangle {
            width: parent.width
            height: 188
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#cc1b1f29" }
                GradientStop { position: 0.55; color: "#771b1f29" }
                GradientStop { position: 1.0; color: "#1b1f29" }
            }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            // The header stacks the title and the action row so a narrow
            // window shrinks nothing and nothing overlaps. The action Flow
            // wraps to a second line instead.
            Column {
                id: headerColumn
                width: parent.width
                spacing: 10

                Column {
                    id: titleColumn
                    width: parent.width
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
                            width: Math.min(implicitWidth,
                                parent.width - (rankedPill.visible ? rankedPill.width + 8 : 0))
                        }
                        Pill {
                            id: rankedPill
                            text: "Ranked"
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
                            text: String(screen.visibleUsers.filter(function(u) { return !u.away && !u.channelAway }).length) + " online"
                            dot: true
                            foreground: theme.green
                            fill: "#0b1512"
                            line: "#245544"
                        }
                        Pill {
                            visible: screen.files.known === true
                            text: screen.files.ready ? "Files downloaded" : "Missing " + (screen.files.missing ?? []).length + " of " + screen.files.total + " files"
                            foreground: screen.files.ready ? theme.green : theme.gold
                        }
                        Pill {
                            text: String(screen.incomingChallenges.length) + " challenges"
                            visible: screen.incomingChallenges.length > 0
                            foreground: theme.gold
                            fill: "#1a160b"
                            line: "#5b4720"
                        }
                    }
                }

                Flow {
                    id: actionsRow
                    width: parent.width
                    spacing: 8
                    layoutDirection: Qt.RightToLeft

                    AppButton {
                        text: "Leave"
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
                        visible: screen.channel.supportsTraining === true
                        text: "Training"
                        compact: true
                        width: 112
                        onClicked: screen.trainingRequested()
                    }
                    AppButton {
                        text: "Controls"
                        compact: true
                        onClicked: screen.controlsRequested()
                    }
                    AppButton {
                        visible: screen.files.known === true && !screen.files.ready
                        text: roms.busy ? "Downloading" : "Download files"
                        busy: roms.busy
                        compact: true
                        onClicked: screen.filesRequested()
                    }
                    AppButton {
                        text: "Test game"
                        compact: true
                        width: 120
                        onClicked: screen.testRequested()
                    }
                }
            }

            Row {
                width: parent.width
                height: parent.height - headerColumn.height - 14
                spacing: 14

                Rectangle {
                    width: Math.max(240, Math.min(320, parent.width * 0.28))
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
                                text: "Players"
                                color: theme.textMuted
                                font.family: theme.displayFamily
                                font.weight: Font.DemiBold
                                font.pixelSize: 12
                            }
                            Item { width: parent.width - 110; height: 1 }
                            Text {
                                text: screen.playerSearch.length > 0
                                      ? screen.visibleUsers.length + " / " + screen.sortedUsers().length
                                      : String(screen.sortedUsers().length) + " in room"
                                color: theme.textFaint
                                font.family: theme.bodyFamily
                                font.pixelSize: 10
                            }
                        }

                        AppButton {
                            width: parent.width
                            height: 34
                            compact: true
                            text: screen.friendsOnly ? "Showing friends" : "Filter friends"
                            selected: screen.friendsOnly
                            onClicked: screen.friendsOnly = !screen.friendsOnly
                        }

                        AppButton {
                            width: parent.width
                            height: 34
                            compact: true
                            primary: screen.autoMatchActive
                            text: screen.autoMatchActive ? "Auto matching..." : "Auto match"
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
                            objectName: "playerList"
                            width: parent.width
                            height: parent.height - 148
                            clip: true
                            spacing: 1
                            model: ListModel { id: playerModel }
                            // FocusController owns selection. With a current
                            // row, a join above it would shift currentIndex
                            // and the view would steal focus and scroll to it.
                            currentIndex: -1
                            boundsBehavior: Flickable.StopAtBounds

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
                                required property string playerKey
                                width: ListView.view.width
                                player: screen.playerLookup[playerKey] ?? ({})
                                isLocal: screen.isLocal(player)
                                isFriend: screen.friends.some(function(name) { return name.toLowerCase() === playerKey.toLowerCase() })
                                onChallengeRequested: screen.challengePlayer(player)
                                onFriendToggleRequested: screen.friendToggleRequested(player)
                                onCopyRequested: screen.copyRequested(playerKey)
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
                            - Math.max(240, Math.min(320, parent.width * 0.28))
                            - Math.max(220, Math.min(300, parent.width * 0.25)) - 28
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
                                text: "Chat"
                                color: theme.textMuted
                                font.family: theme.displayFamily
                                font.weight: Font.DemiBold
                                font.pixelSize: 12
                            }
                            Item { width: parent.width - 100; height: 1 }
                            Text {
                                text: "Live"
                                color: theme.green
                                font.family: theme.bodyFamily
                                font.weight: Font.DemiBold
                                font.pixelSize: 10
                            }
                        }

                        ListView {
                            id: chatList
                            objectName: "chatList"
                            width: parent.width
                            height: parent.height - 86
                            clip: true
                            spacing: 6
                            model: ListModel { id: chatModel }
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: Item {
                                id: messageRow
                                required property string kind
                                required property string username
                                required property string body
                                width: chatList.width
                                height: messageText.implicitHeight + 8

                                Text {
                                    id: messageText
                                    width: parent.width
                                    textFormat: Text.PlainText
                                    text: messageRow.kind === "system"
                                          ? "— " + messageRow.body
                                          : messageRow.username + "   " + messageRow.body
                                    color: messageRow.kind === "system" ? theme.textFaint
                                         : messageRow.kind === "error" ? theme.red
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
                                text: "Send"
                                compact: true
                                width: 96
                                height: 46
                                onClicked: screen.sendMessage()
                            }
                        }
                    }
                }

                Rectangle {
                    width: Math.max(220, Math.min(300, parent.width * 0.25))
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
                            text: "Live matches"
                            color: theme.textMuted
                            font.family: theme.displayFamily
                            font.weight: Font.DemiBold
                            font.pixelSize: 12
                        }

                        AppButton {
                            width: parent.width
                            height: 30
                            compact: true
                            text: "Watch top match"
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
                            // Keyed by match so spectator-count and roster
                            // updates don't rebuild a selected row.
                            model: screen.liveMatchKeys

                            delegate: Focusable {
                                id: matchItem
                                required property string modelData
                                readonly property var match: screen.matchByKey(modelData)
                                width: parent.width
                                height: 58
                                ringRadius: 9
                                onActivated: screen.spectateRequested(match)

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
                                        text: screen.matchPlayerNames(matchItem.match)
                                        color: theme.text
                                        elide: Text.ElideRight
                                        font.family: theme.displayFamily
                                        font.weight: Font.DemiBold
                                        font.pixelSize: 14
                                    }
                                    Text {
                                        text: (matchItem.match.ranked > 0 ? "FT" + matchItem.match.ranked + "  ·  " : "CASUAL  ·  ")
                                              + matchItem.match.spectators + " watching"
                                        color: theme.textFaint
                                        font.family: theme.bodyFamily
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        Item { width: 1; height: 2 }

                        Text {
                            text: "Channel notes"
                            visible: (screen.channel.motd ?? "").length > 0
                            color: theme.textMuted
                            font.family: theme.displayFamily
                            font.weight: Font.DemiBold
                            font.pixelSize: 12
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
