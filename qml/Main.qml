import QtQuick
import QtQuick.Window
import QtQuick.Controls
import "components"
import "screens"

ApplicationWindow {
    id: root

    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 640
    title: "Deckade"
    color: theme.background
    readonly property bool startWindowed: Qt.application.arguments.indexOf("--windowed") !== -1

    property string screen: "home"
    property var user: ({})
    property var appInfo: ({})
    property var joinedChannels: []
    property var allChannels: []
    property var welcomeSections: []
    property var filters: ({ genres: [], years: [], systems: [] })
    property var currentChannel: null
    property var messagesByChannel: ({})
    property var currentMessages: []
    property var incomingChallenges: []
    property var outgoingChallenges: []
    property var activeIncoming: null
    property var toasts: []
    property var unread: ({})

    property var browseChannels: []
    property int browsePage: 0
    property int browseTotal: 0
    property string browseTitle: "ALL GAMES"
    property bool browseLoading: false
    property var browseQuery: ({})

    property bool loginBusy: false
    property string loginBusyText: "SIGNING IN"
    property string loginError: ""
    property bool autoLoginAttempt: false

    property var autoJoinNames: []
    property string pendingOpenChannel: ""
    property int nextChallengeId: 1

    property bool challengeDialogVisible: false
    property bool challengeIncoming: false
    property var challengeTarget: ({})
    property var challengeData: ({})

    property bool launching: false
    property bool autoMatchActive: false
    property string autoMatchTarget: ""
    property string launchingTitle: ""
    property var pendingChat: null
    property bool virtualKeyboardVisible: false
    property bool hidLauncher: false
    property var pendingRomAction: null
    property double suppressGamepadUntil: 0

    function notify(message, level) {
        if (!message || message.length === 0) return
        var list = toasts.slice()
        list.push({ id: Date.now() + Math.random(), message: message, level: level ?? "info" })
        if (list.length > 4) list.shift()
        toasts = list
    }

    function removeToast(id) {
        toasts = toasts.filter(function(toast) { return toast.id !== id })
    }

    function gamepadAllowed() {
        // While a native emulator is running, every gamepad event belongs to
        // the emulator. Never let the launcher act on the same press again.
        if (launcher.running) return false
        if (controllerProfile.capturing) return false
        return Date.now() >= root.suppressGamepadUntil
    }

    function closeTransientUi() {
        if (virtualKeyboardVisible) virtualKeyboard.close()
        if (challengeDialogVisible) {
            challengeDialog.close()
            challengeDialogVisible = false
            challengeIncoming = false
            activeIncoming = null
        }
        focusController.activeGroup = "main"
    }

    function isFavorite(name) {
        return ((user.favoritesChannels ?? [])).indexOf(name) !== -1
    }

    function roomLimit() {
        return user.perks?.maxNumChannels ?? 5
    }

    function isJoined(name) {
        return joinedChannels.some(function(channel) { return channel.name === name })
    }

    function channelByName(name) {
        return joinedChannels.find(function(channel) { return channel.name === name }) ?? null
    }

    function messagesFor(name) {
        return messagesByChannel[name] ?? []
    }

    function appendMessage(name, message) {
        var copy = Object.assign({}, messagesByChannel)
        var list = (copy[name] ?? []).slice()
        list.push(message)
        if (list.length > 400) list.shift()
        copy[name] = list
        messagesByChannel = copy
        if (currentChannel && currentChannel.name === name) {
            currentMessages = list
        }
        if ((currentChannel?.name ?? "") !== name && message.kind === "chat") {
            var u = Object.assign({}, unread)
            u[name] = (u[name] ?? 0) + 1
            unread = u
        }
    }

    function appendSystemMessage(channelName, text) {
        if (!channelName || !text) return
        appendMessage(channelName, {
            id: Date.now() + Math.random(),
            channelName: channelName,
            username: "system",
            text: text,
            kind: "system",
            timestamp: Date.now()
        })
    }

    function replaceChannel(name, updater) {
        var list = joinedChannels.slice()
        var found = null
        for (var i = 0; i < list.length; ++i) {
            if (list[i].name === name) {
                var copy = Object.assign({}, list[i])
                updater(copy)
                list[i] = copy
                found = copy
                break
            }
        }
        if (found) {
            joinedChannels = list
            if (currentChannel && currentChannel.name === name) {
                currentChannel = found
            }
        }
    }

    function addJoinedChannel(channel) {
        var list = joinedChannels.filter(function(item) { return item.name !== channel.name })
        list.push(channel)
        joinedChannels = list
        if (!messagesByChannel[channel.name]) {
            appendMessage(channel.name, {
                kind: "system",
                text: "Joined #" + channel.name,
                username: "system",
                timestamp: Date.now()
            })
        }
    }

    function switchScreen(next) {
        if (next === "room" && !currentChannel) {
            notify("Join a room first.", "warning")
            return
        }
        screen = next
        focusController.activeGroup = "main"
        Qt.callLater(function() {
            if (next === "home") homeScreen.focusDefault()
            else if (next === "browse") browseScreen.focusSearch()
            else if (next === "room") roomScreen.focusDefault()
            else if (next === "rooms") roomsScreen.focusDefault()
            else if (next === "controls") controlsScreen.focusDefault()
            else if (next === "settings") settingsScreen.focusDefault()
            else focusController.focusFirst()
        })
    }

    function openRoom(name) {
        var channel = channelByName(name)
        if (!channel) {
            pendingOpenChannel = name
            client.joinChannel(name, false)
            return
        }
        currentChannel = channel
        currentMessages = messagesFor(name)
        unread = Object.assign({}, unread, { [name]: 0 })
        settings.rememberChannel(name)
        switchScreen("room")
    }

    function joinChannel(name, auto) {
        if (!name) return
        if (isJoined(name)) {
            if (!auto) openRoom(name)
            return
        }
        if (joinedChannels.length >= roomLimit()) {
            notify("Room limit reached (" + roomLimit() + "). Leave a room before joining another.", "warning")
            return
        }
        if (!auto) pendingOpenChannel = name
        client.joinChannel(name, false)
    }

    function leaveRoom() {
        if (!currentChannel) return
        var name = currentChannel.name
        client.leaveChannel(name)
        joinedChannels = joinedChannels.filter(function(channel) { return channel.name !== name })
        currentChannel = null
        currentMessages = []
        switchScreen("home")
    }

    function toggleFavorite(name, desired) {
        var next = desired === undefined ? !isFavorite(name) : desired
        client.favChannel(name, next)
        replaceChannel(name, function(channel) { channel.isFavorite = next })
        var list = allChannels.slice()
        for (var i = 0; i < list.length; ++i) {
            if (list[i].name === name) {
                list[i] = Object.assign({}, list[i], { isFavorite: next })
            }
        }
        allChannels = list
    }

    function sendChat(text) {
        if (!currentChannel) return
        const channelName = currentChannel.name
        const message = {
            id: Date.now() + Math.random(),
            channelName: channelName,
            username: user.name ?? "",
            text: text,
            kind: "chat",
            timestamp: Date.now(),
            pending: true
        }
        // Fightcade's own client paints your message immediately and filters
        // the server echo. Do the same so a successful send has visible
        // feedback even if the channel echo is delayed.
        appendMessage(channelName, message)
        pendingChat = message
        client.sendChat(channelName, text)
    }

    function finishPendingChat(sent) {
        if (!pendingChat) return
        const pending = pendingChat
        pendingChat = null
        if (sent) return
        const list = (messagesFor(pending.channelName) ?? []).filter(function(item) {
            return item.id !== pending.id
        })
        var copy = Object.assign({}, messagesByChannel)
        copy[pending.channelName] = list
        messagesByChannel = copy
        if (currentChannel && currentChannel.name === pending.channelName) {
            currentMessages = list
        }
    }

    function sendChallenge(target, ranked) {
        if (!currentChannel || !target) return
        var id = nextChallengeId++
        client.challenge(target.name, currentChannel.name, id, ranked)
    }

    function autoMatchCandidates() {
        if (!currentChannel) return []
        var activeNames = (currentChannel.users ?? []).filter(function(player) {
            return !player.away && !player.channelAway
                && !(player.playing && player.playing.quarkId)
                && player.name !== user.name
                && !incomingChallenges.some(function(challenge) { return challenge.challenger === player.name })
                && !outgoingChallenges.some(function(challenge) { return challenge.challenger === player.name })
        })
        return activeNames
    }

    function stopAutoMatch(cancelPending) {
        autoMatchActive = false
        autoMatchTimer.stop()
        if (cancelPending && autoMatchTarget.length > 0 && currentChannel) {
            var pending = outgoingChallenges.find(function(challenge) { return challenge.challenger === autoMatchTarget })
            if (pending) client.cancelChallenge(pending.challenger, pending.channelName, pending.challengeId)
        }
        autoMatchTarget = ""
    }

    function nextAutoMatch() {
        if (!autoMatchActive || !currentChannel) {
            stopAutoMatch(false)
            return
        }
        var candidates = autoMatchCandidates()
        if (candidates.length === 0) {
            stopAutoMatch(false)
            notify("Auto-match found no available players.", "warning")
            return
        }
        var target = candidates[0]
        autoMatchTarget = target.name
        var ranked = (currentChannel.isRanked && (user.settings?.ranked ?? false)) ? 3 : 0
        sendChallenge(target, ranked)
        autoMatchTimer.restart()
    }

    function toggleAutoMatch() {
        if (autoMatchActive) {
            stopAutoMatch(true)
            notify("Auto-match canceled.", "info")
        } else {
            autoMatchActive = true
            notify("Auto-match looking for an opponent...", "info")
            nextAutoMatch()
        }
    }

    function openChallengeForPlayer(player) {
        if (!currentChannel) return
        if (!player) return
        if (player.name === user.name) {
            notify("That is you.", "warning")
            return
        }
        if (player.playing && player.playing.quarkId) {
            notify(player.name + " is already playing.", "warning")
            return
        }
        if (player.away || player.channelAway) {
            notify(player.name + " is away.", "warning")
            return
        }
        challengeIncoming = false
        challengeTarget = player
        challengeData = {}
        challengeDialogVisible = true
        var defaultRanked = (currentChannel.isRanked && (user.settings?.ranked ?? false)) ? 3 : 0
        challengeDialog.open(false, player, {}, defaultRanked)
    }

    function openIncomingChallenge(challenge) {
        if (!challenge) return
        var channel = channelByName(challenge.channelName)
        var player = (channel?.users ?? []).find(function(item) { return item.name === challenge.challenger }) ?? ({
            name: challenge.challenger,
            gravatar: "",
            away: false,
            playing: {}
        })
        activeIncoming = challenge
        challengeIncoming = true
        challengeTarget = player
        challengeData = challenge
        challengeDialogVisible = true
        challengeDialog.open(true, player, challenge, challenge.ranked)
    }

    function nextIncomingChallenge() {
        activeIncoming = null
        if (incomingChallenges.length > 0) {
            var next = incomingChallenges[0]
            Qt.callLater(function() { openIncomingChallenge(next) })
        }
    }

    function acceptActiveChallenge(ranked) {
        if (challengeIncoming && activeIncoming) {
            client.acceptChallenge(activeIncoming.challenger, activeIncoming.channelName,
                                   activeIncoming.challengeId, ranked)
            var current = activeIncoming
            incomingChallenges = incomingChallenges.filter(function(item) {
                return !(item.challengeId === current.challengeId && item.challenger === current.challenger)
            })
            activeIncoming = null
            challengeDialogVisible = false
            focusController.activeGroup = "main"
            nextIncomingChallenge()
            return
        }
        if (challengeTarget) {
            sendChallenge(challengeTarget, ranked)
        }
        challengeDialogVisible = false
        focusController.activeGroup = "main"
    }

    function declineActiveChallenge() {
        if (challengeIncoming && activeIncoming) {
            client.rejectChallenge(activeIncoming.challenger, activeIncoming.channelName,
                                   activeIncoming.challengeId)
            var current = activeIncoming
            incomingChallenges = incomingChallenges.filter(function(item) {
                return !(item.challengeId === current.challengeId && item.challenger === current.challenger)
            })
            activeIncoming = null
            challengeDialogVisible = false
            focusController.activeGroup = "main"
            nextIncomingChallenge()
        } else {
            challengeDialogVisible = false
            focusController.activeGroup = "main"
        }
    }

    function cancelChallengeDialog() {
        challengeDialogVisible = false
        focusController.activeGroup = "main"
    }

    function launchSoloNow(emulatorName, gameId) {
        if (launcher.launchSolo(emulatorName, gameId)) {
            launching = true
            launchingTitle = currentChannel?.title ?? currentChannel?.name ?? gameId
        } else {
            notify(launcher.lastError, "error")
        }
    }

    function launchTrainingNow(emulatorName, gameId) {
        if (launcher.launchTraining(emulatorName, gameId)) {
            launching = true
            launchingTitle = currentChannel?.title ?? currentChannel?.name ?? gameId
        } else {
            notify(launcher.lastError, "error")
        }
    }

    function requestRom(action) {
        if (!action || !action.emulator || !action.gameId) {
            notify("This channel does not expose a usable emulator/game ID.", "error")
            return
        }
        const present = roms.hasRom(action.emulator, action.gameId)
        if (!present && !settings.autoDownloadRoms) {
            notify("ROM missing. Add it to the ROM folder or enable automatic ROM downloads.", "error")
            return
        }
        pendingRomAction = action
        if (!present) notify("Downloading ROM for " + action.gameId + "...", "info")
        roms.ensureRom(action.emulator, action.gameId)
    }

    function executeRomAction(action) {
        if (!action) return
        root.closeTransientUi()
        root.suppressGamepadUntil = Date.now() + 1200
        if (action.type === "solo") {
            launchSoloNow(action.emulator, action.gameId)
        } else if (action.type === "training") {
            launchTrainingNow(action.emulator, action.gameId)
        } else if (action.type === "spectate") {
            if (launcher.launchSpectate(action.payload)) {
                launching = true
                launchingTitle = action.title ?? action.payload.gameId
            } else {
                notify(launcher.lastError, "error")
            }
        } else if (action.type === "match") {
            if (launcher.launchMatch(action.payload)) {
                launching = true
                launchingTitle = action.title ?? action.payload.gameId
            } else {
                notify(launcher.lastError, "error")
            }
        }
    }

    function startTestGame() {
        if (!currentChannel) return
        requestRom({
            type: "solo",
            emulator: currentChannel.emulator,
            gameId: currentChannel.gameId
        })
    }

    function startTraining() {
        if (!currentChannel) return
        requestRom({
            type: "training",
            emulator: currentChannel.emulator,
            gameId: currentChannel.gameId
        })
    }

    function spectateMatch(quark) {
        if (!currentChannel) return
        var payload = Object.assign({}, quark)
        payload.emulator = currentChannel.emulator
        payload.gameId = currentChannel.gameId
        requestRom({
            type: "spectate",
            emulator: currentChannel.emulator,
            gameId: currentChannel.gameId,
            title: currentChannel.title ?? currentChannel.name,
            payload: payload
        })
    }

    function startChannelTv() {
        if (!currentChannel) return
        var quarks = currentChannel.quarks ?? []
        if (quarks.length === 0) {
            notify("No live match to watch in this room.", "warning")
            return
        }
        spectateMatch(quarks[0])
    }

    function revealItem(item) {
        if (!item) return
        var parent = item.parent
        while (parent) {
            if (parent.contentY !== undefined && parent.contentItem !== undefined) {
                var pos = item.mapToItem(parent.contentItem, 0, 0)
                var margin = 26
                if (pos.y < parent.contentY + margin) {
                    parent.contentY = Math.max(0, pos.y - margin)
                } else if (pos.y + item.height > parent.contentY + parent.height - margin) {
                    parent.contentY = pos.y + item.height - parent.height + margin
                }
            }
            parent = parent.parent
        }
    }

    function isTextInputFocused() {
        var item = root.activeFocusItem
        return item && (item.echoMode !== undefined || item.deckTextInput === true)
    }

    function openKeyboard() {
        var target = root.activeFocusItem
        if (target && (target.echoMode !== undefined || target.deckTextInput === true)) {
            virtualKeyboardVisible = true
            virtualKeyboard.open(target)
        } else {
            notify("Focus a text field first, then press Y.", "info")
        }
    }

    function cycleSection(direction) {
        var sections = ["home", "browse"]
        if (joinedChannels.length > 0) sections.push("rooms")
        sections.push("settings")
        var index = sections.indexOf(screen)
        if (index < 0) index = 0
        index = (index + direction + sections.length) % sections.length
        switchScreen(sections[index])
    }

    function goBack() {
        if (virtualKeyboardVisible) {
            virtualKeyboard.close()
            return
        }
        if (challengeDialogVisible) {
            declineActiveChallenge()
            return
        }
        if (screen === "controls") {
            if (currentChannel) switchScreen("room")
            else switchScreen("home")
            return
        }
        if (screen === "room" || screen === "rooms" || screen === "browse" || screen === "settings") {
            switchScreen("home")
        }
    }

    function browseRequest(request) {
        browseLoading = true
        browseQuery = request
        browsePage = request.page ?? 0
        browseTitle = (request.text ?? "").length > 0 ? "SEARCH RESULTS"
                    : request.favorites === true ? "FAVORITES"
                    : request.ranked === true ? "RANKED GAMES"
                    : request.ranked === false ? "UNRANKED GAMES"
                    : "ALL GAMES"
        client.listChannels(request)
    }

    function handleLoggedIn() {
        loginBusy = false
        autoLoginAttempt = false
        loginError = ""
        screen = "home"
        focusController.activeGroup = "main"
        Qt.callLater(function() { homeScreen.focusDefault() })
        client.listWelcome()
        client.listAllChannels()
        client.listFilters()
    }

    function autoJoinRememberedChannels() {
        if (!settings.autoJoinLastChannels) return
        const known = allChannels.map(function(channel) { return channel.name })

        // The Fightcade account remembers channels server-side. Restore those
        // first; this is what makes all of a user's active rooms reappear after
        // signing in on another device.
        const serverActive = (user.activeChannels ?? []).map(function(entry) {
            return entry.channelName
        }).filter(function(name) { return !!name })

        const localKnown = (settings.lastChannels ?? []).filter(function(name) {
            return known.indexOf(name) !== -1
        })
        if (localKnown.length !== (settings.lastChannels ?? []).length) {
            settings.lastChannels = localKnown
        }

        const candidates = []
        const seen = ({})
        for (var i = 0; i < serverActive.length; ++i) {
            if (!seen[serverActive[i]]) { seen[serverActive[i]] = true; candidates.push(serverActive[i]) }
        }
        for (var j = 0; j < localKnown.length; ++j) {
            if (!seen[localKnown[j]]) { seen[localKnown[j]] = true; candidates.push(localKnown[j]) }
        }

        const freeSlots = Math.max(0, roomLimit() - joinedChannels.length)
        autoJoinNames = candidates.slice(0, Math.min(6, freeSlots))
        for (var k = 0; k < autoJoinNames.length; ++k) {
            if (!isJoined(autoJoinNames[k])) client.joinChannel(autoJoinNames[k], false)
        }
    }

    function handleLoginFailure(code, message) {
        loginBusy = false
        loginError = message
        if (autoLoginAttempt) {
            autoLoginAttempt = false
            if (["wrong-password", "user-not-found", "user-locked", "user-banned",
                 "login-locked", "too-many-failed-login"].indexOf(code) !== -1) {
                settings.clearSession()
            }
        }
    }

    function updateUserInChannels(update) {
        var username = update.username ?? ""
        if (!username) return
        for (var c = 0; c < joinedChannels.length; ++c) {
            var channel = joinedChannels[c]
            if (update.channelName && update.channelName.length > 0 && update.channelName !== channel.name) {
                continue
            }
            var users = channel.users ?? []
            var index = -1
            for (var i = 0; i < users.length; ++i) {
                if (users[i].name === username) { index = i; break }
            }
            if (index < 0) continue
            var nextUsers = users.slice()
            var player = Object.assign({}, users[index])
            if (update.away !== undefined && (update.channelAway === undefined || update.channelName === channel.name)) {
                player.away = update.away
            }
            if (update.playing !== undefined) {
                player.playing = update.playing
            }
            if (update.rank !== undefined) {
                player.rank = update.rank
            }
            if (update.wifi !== undefined) {
                player.isWifi = update.wifi
            }
            if (update.channelAway !== undefined) {
                player.channelAway = update.channelAway
            }
            nextUsers[index] = player
            var list = joinedChannels.slice()
            var channelCopy = Object.assign({}, channel, { users: nextUsers })
            list[c] = channelCopy
            joinedChannels = list
            if (currentChannel && currentChannel.name === channelCopy.name) {
                currentChannel = channelCopy
            }
            break
        }
    }

    function applyWindowMode() {
        // Delay the visibility change until the window has been mapped. This
        // avoids a fullscreen flicker on startup and makes --windowed reliable.
        windowModeTimer.restart()
    }

    Component.onCompleted: {
        applyWindowMode()
        if (settings.hasSession && settings.rememberLogin) {
            loginBusy = true
            loginBusyText = "RESTORING SESSION"
            autoLoginAttempt = true
            client.autologin(settings.sessionCookie)
        }
    }

    Connections {
        target: settings
        function onFullscreenChanged() { root.applyWindowMode() }
    }

    function isTextFieldFocused() {
        return root.isTextInputFocused()
    }

    Shortcut {
        sequence: "F11"
        onActivated: appController.toggleFullscreen()
    }
    Shortcut {
        sequence: "F10"
        onActivated: appController.setWindowMinimized(true)
    }
    Shortcut {
        sequence: "Up"
        enabled: !root.isTextFieldFocused()
        onActivated: focusController.move(0, -1)
    }
    Shortcut {
        sequence: "Down"
        enabled: !root.isTextFieldFocused()
        onActivated: focusController.move(0, 1)
    }
    Shortcut {
        sequence: "Left"
        enabled: !root.isTextFieldFocused()
        onActivated: focusController.move(-1, 0)
    }
    Shortcut {
        sequence: "Right"
        enabled: !root.isTextFieldFocused()
        onActivated: focusController.move(1, 0)
    }
    Shortcut {
        sequence: "Return"
        enabled: !root.isTextFieldFocused()
        onActivated: focusController.activate()
    }
    Shortcut {
        sequence: "Enter"
        enabled: !root.isTextFieldFocused()
        onActivated: focusController.activate()
    }
    Shortcut {
        sequence: "Space"
        enabled: !root.isTextFieldFocused()
        onActivated: focusController.activate()
    }
    Shortcut {
        sequence: "Escape"
        onActivated: root.goBack()
    }

    Connections {
        target: gamepad
        function onNavigate(dx, dy) {
            if (!root.gamepadAllowed() || controllerProfile.capturing) return
            focusController.move(dx, dy)
        }
        function onAcceptPressed() {
            if (!root.gamepadAllowed() || controllerProfile.capturing) return
            if (root.isTextInputFocused()) root.openKeyboard()
            else focusController.activate()
        }
        function onBackPressed() {
            if (!root.gamepadAllowed()) return
            if (controllerProfile.capturing) {
                controllerProfile.cancelCapture()
                return
            }
            root.goBack()
        }
        function onSecondaryPressed() {
            if (!root.gamepadAllowed() || controllerProfile.capturing) return
            focusController.secondaryActivate()
        }
        function onKeyboardPressed() {
            if (root.gamepadAllowed()) root.openKeyboard()
        }
        function onMenuPressed() {
            if (controllerProfile.capturing) {
                controllerProfile.cancelCapture()
                return
            }
            if (root.gamepadAllowed()) root.switchScreen("settings")
        }
        function onTabLeftPressed() {
            if (root.gamepadAllowed()) root.cycleSection(-1)
        }
        function onTabRightPressed() {
            if (root.gamepadAllowed()) root.cycleSection(1)
        }
    }

    Connections {
        target: focusController
        function onFocusMoved(item) { root.revealItem(item) }
    }

    Connections {
        target: client
        function onLoggedIn(loggedUser, info) {
            root.user = loggedUser
            root.appInfo = info
            root.handleLoggedIn()
        }
        function onLoginFailed(code, message) { root.handleLoginFailure(code, message) }
        function onLoggedOut() {
            root.user = {}
            root.joinedChannels = []
            root.currentChannel = null
            root.currentMessages = []
            root.allChannels = []
            root.welcomeSections = []
            root.incomingChallenges = []
            root.outgoingChallenges = []
            root.messagesByChannel = {}
            root.screen = "home"
            root.loginBusy = false
            root.loginError = ""
        }
        function onConnectionLost(message) {
            root.notify(message, "error")
            root.loginError = message
        }
        function onUserChanged() { root.user = client.user }
        function onWelcomeLoaded(sections) { root.welcomeSections = sections }
        function onAllChannelsLoaded(channels) {
            root.allChannels = channels
            root.autoJoinRememberedChannels()
        }
        function onChannelsLoaded(channels, page, totalChannels) {
            root.browseChannels = channels
            root.browsePage = page
            root.browseTotal = totalChannels
            root.browseLoading = false
        }
        function onFiltersLoaded(options) { root.filters = options }
        function onChannelJoined(channel) {
            root.addJoinedChannel(channel)
            root.browseLoading = false
            if (root.pendingOpenChannel === channel.name) {
                root.pendingOpenChannel = ""
                root.openRoom(channel.name)
            } else if (root.autoJoinNames.indexOf(channel.name) !== -1) {
                root.autoJoinNames = root.autoJoinNames.filter(function(name) { return name !== channel.name })
            }
            settings.rememberChannel(channel.name)
        }
        function onChannelJoinFailed(channelName, message) {
            root.browseLoading = false
            const wasAuto = root.autoJoinNames.indexOf(channelName) !== -1
            root.autoJoinNames = root.autoJoinNames.filter(function(name) { return name !== channelName })
            if (wasAuto) {
                // Remembered room names can become stale. Drop invalid ones instead
                // of retrying and spamming an error every launch.
                settings.lastChannels = (settings.lastChannels ?? []).filter(function(name) { return name !== channelName })
            }
            if (root.pendingOpenChannel === channelName) root.pendingOpenChannel = ""
            if (!wasAuto) root.notify("Could not join #" + channelName + ": " + message, "error")
        }
        function onChannelLeft(channelName) {
            root.joinedChannels = root.joinedChannels.filter(function(channel) { return channel.name !== channelName })
            settings.lastChannels = (settings.lastChannels ?? []).filter(function(name) { return name !== channelName })
            if (root.currentChannel && root.currentChannel.name === channelName) {
                root.currentChannel = null
                root.currentMessages = []
                root.switchScreen("home")
            }
        }
        function onChatReceived(message) {
            if ((message.username ?? "") === (root.user.name ?? "")) return
            root.appendMessage(message.channelName, message)
        }
        function onChatSent(channelName) { root.finishPendingChat(true) }
        function onChatFailed(channelName, message) {
            root.finishPendingChat(false)
            root.notify(message, "error")
        }
        function onServerNotice(message) { root.notify(message, "error") }
        function onUserJoined(channelName, joinedUser) {
            root.replaceChannel(channelName, function(channel) {
                var users = (channel.users ?? []).filter(function(item) { return item.name !== joinedUser.name })
                users.push(joinedUser)
                channel.users = users
                channel.playerCount = users.length
            })
        }
        function onUserLeft(channelName, username) {
            root.replaceChannel(channelName, function(channel) {
                channel.users = (channel.users ?? []).filter(function(item) { return item.name !== username })
                channel.playerCount = channel.users.length
            })
        }
        function onUserKicked(channelName) {
            root.notify("You were removed from #" + channelName, "warning")
            root.joinedChannels = root.joinedChannels.filter(function(channel) { return channel.name !== channelName })
            if (root.currentChannel && root.currentChannel.name === channelName) {
                root.currentChannel = null
                root.switchScreen("home")
            }
        }
        function onUserUpdated(update) { root.updateUserInChannels(update) }
        function onChallengeReceived(challenge) {
            var exists = root.incomingChallenges.some(function(item) {
                return item.challenger === challenge.challenger && item.challengeId === challenge.challengeId
            })
            if (!exists) root.incomingChallenges = root.incomingChallenges.concat([challenge])
            if (!root.challengeDialogVisible) root.openIncomingChallenge(challenge)
        }
        function onChallengeCancelled(challenge) {
            if (root.autoMatchActive && challenge.challenger === root.autoMatchTarget) {
                root.autoMatchTarget = ""
                root.autoMatchTimer.restart()
            }
            var wasOutgoing = root.outgoingChallenges.some(function(item) {
                return item.challenger === challenge.challenger && item.challengeId === challenge.challengeId
            })
            root.incomingChallenges = root.incomingChallenges.filter(function(item) {
                return !(item.challenger === challenge.challenger && item.challengeId === challenge.challengeId)
            })
            root.outgoingChallenges = root.outgoingChallenges.filter(function(item) {
                return !(item.challenger === challenge.challenger && item.challengeId === challenge.challengeId)
            })
            if (wasOutgoing) {
                root.notify("Challenge to " + challenge.challenger + " canceled.", "info")
                root.appendSystemMessage(challenge.channelName,
                    "Challenge to " + challenge.challenger + " was canceled.")
            }
            if (root.challengeIncoming && root.activeIncoming
                && root.activeIncoming.challenger === challenge.challenger
                && root.activeIncoming.challengeId === challenge.challengeId) {
                root.challengeDialogVisible = false
                root.activeIncoming = null
                challengeDialog.close()
                focusController.activeGroup = "main"
                root.nextIncomingChallenge()
            }
        }
        function onChallengeRejected(challenge) {
            if (root.autoMatchActive && challenge.challenger === root.autoMatchTarget) {
                root.autoMatchTarget = ""
                root.autoMatchTimer.restart()
            }
            root.outgoingChallenges = root.outgoingChallenges.filter(function(item) {
                return !(item.challenger === challenge.challenger && item.challengeId === challenge.challengeId)
            })
            root.notify(challenge.challenger + " declined the challenge.", "warning")
            root.appendSystemMessage(challenge.channelName,
                challenge.challenger + " declined the challenge.")
        }
        function onChallengeAccepted(challenge) {
            if (root.autoMatchActive) root.stopAutoMatch(false)
            root.outgoingChallenges = root.outgoingChallenges.filter(function(item) {
                return !(item.challenger === challenge.challenger && item.challengeId === challenge.challengeId)
            })
            root.notify(challenge.challenger + " accepted. Starting match...", "info")
            root.appendSystemMessage(challenge.channelName,
                challenge.challenger + " accepted the challenge.")
        }
        function onChallengeSent(challenge) {
            root.outgoingChallenges = root.outgoingChallenges.concat([challenge])
            root.notify("Challenge sent to " + challenge.challenger + " — waiting for a response.", "info")
            root.appendSystemMessage(challenge.channelName,
                "Challenge sent to " + challenge.challenger + ". Waiting for a response...")
        }
        function onChallengeFailed(channelName, message) {
            if (root.autoMatchActive) {
                root.autoMatchTarget = ""
                root.autoMatchTimer.restart()
            }
            root.notify(message, "error")
        }
        function onMatchStarted(match) {
            if (root.autoMatchActive) root.stopAutoMatch(false)
            matchHistory.recordMatch(match)
            var channel = root.channelByName(match.channelName) ?? root.currentChannel
            var payload = Object.assign({}, match)
            if (!payload.emulator) payload.emulator = channel?.emulator
            if (!payload.gameId) payload.gameId = channel?.gameId
            root.requestRom({
                type: "match",
                emulator: payload.emulator,
                gameId: payload.gameId,
                title: channel?.title ?? channel?.name ?? "Match",
                payload: payload
            })
        }
        function onQuarkStats(channelName, quarkId, spectators) {
            // Update the live-match metadata in place. Replacing the channel
            // object here made the player ListView re-evaluate its model every
            // couple of seconds and jump back to the top.
            for (var c = 0; c < root.joinedChannels.length; ++c) {
                var channel = root.joinedChannels[c]
                if (channel.name !== channelName) continue
                var quarks = (channel.quarks ?? []).slice()
                for (var i = 0; i < quarks.length; ++i) {
                    if (quarks[i].quarkId === quarkId) {
                        quarks[i] = Object.assign({}, quarks[i], { spectators: spectators })
                    }
                }
                channel.quarks = quarks
                if (root.currentChannel && root.currentChannel.name === channelName) {
                    root.currentChannel.quarks = quarks
                }
                break
            }
        }
        function onNotice(level, message) { root.notify(message, level) }
        function onRateLimited(until) { root.notify("Fightcade rate limit active. Try again in a moment.", "warning") }
        function onRequestFailed(context, message) {
            if (context !== "channels") root.notify(message, "error")
            root.browseLoading = false
        }
    }

    Connections {
        target: roms
        function onRomReady(emulator, gameId, path) {
            if (root.pendingRomAction
                && root.pendingRomAction.emulator === emulator
                && root.pendingRomAction.gameId === gameId) {
                const action = root.pendingRomAction
                root.pendingRomAction = null
                root.executeRomAction(action)
            } else {
                root.notify("ROM ready: " + gameId, "info")
            }
        }
        function onRomFailed(emulator, gameId, message) {
            if (root.pendingRomAction
                && root.pendingRomAction.emulator === emulator
                && root.pendingRomAction.gameId === gameId) {
                root.pendingRomAction = null
            }
            root.notify(message, "error")
        }
        function onNotice(message) { root.notify(message, "info") }
    }

    Connections {
        target: launcher
        function onStateChanged() {
            if (launcher.running) {
                hideLauncherTimer.restart()
            } else {
                hideLauncherTimer.stop()
                if (root.hidLauncher) {
                    root.hidLauncher = false
                    root.closeTransientUi()
                    root.suppressGamepadUntil = Date.now() + 1200
                    appController.setWindowVisible(true)
                    root.applyWindowMode()
                }
                if (root.launching) {
                    root.launching = false
                    root.notify("Emulator closed.", "info")
                }
            }
        }
        function onErrorOccurred(message) {
            appController.setWindowVisible(true)
            root.applyWindowMode()
            root.notify(message, "error")
        }
    }

    Timer {
        id: autoMatchTimer
        interval: 12000
        repeat: false
        onTriggered: root.nextAutoMatch()
    }

    Timer {
        id: hideLauncherTimer
        interval: 1100
        onTriggered: {
            if (launcher.running) {
                root.closeTransientUi()
                root.suppressGamepadUntil = Date.now() + 1200
                root.hidLauncher = true
                appController.setWindowVisible(false)
            }
        }
    }

    Timer {
        id: windowModeTimer
        interval: 150
        onTriggered: {
            visibility = (settings.fullscreen && !root.startWindowed) ? Window.FullScreen : Window.Windowed
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#080a10" }
            GradientStop { position: 0.55; color: "#0b1019" }
            GradientStop { position: 1.0; color: "#111828" }
        }
    }

    Rectangle {
        width: 700
        height: 700
        radius: 350
        x: -360
        y: -420
        color: "#12ff4655"
    }

    Rectangle {
        width: 620
        height: 620
        radius: 310
        anchors.right: parent.right
        anchors.rightMargin: -340
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -360
        color: "#103dd6ff"
    }

    LoginScreen {
        anchors.fill: parent
        visible: !client.connected
        scale: settings.uiScale
        transformOrigin: Item.Center
        restoring: root.loginBusy
        busyText: root.loginBusyText
        errorText: root.loginError
        statusText: client.statusMessage
        onSubmit: function(username, password) {
            root.loginBusy = true
            root.loginBusyText = "SIGNING IN"
            root.loginError = ""
            root.autoLoginAttempt = false
            client.login(username, password)
        }
    }

    Item {
        id: shell
        anchors.fill: parent
        visible: client.connected
        scale: settings.uiScale
        transformOrigin: Item.Center

        Row {
            anchors.fill: parent
            anchors.bottomMargin: settings.showControllerHints ? 42 : 0

            Sidebar {
                width: 94
                height: parent.height
                currentIndex: root.screen === "home" ? 0
                            : root.screen === "browse" ? 1
                            : (root.screen === "room" || root.screen === "rooms") ? 2 : 3
                roomCount: root.joinedChannels.length
                onSelected: function(index) {
                    if (index === 0) root.switchScreen("home")
                    else if (index === 1) root.switchScreen("browse")
                    else if (index === 2) root.switchScreen("rooms")
                    else root.switchScreen("settings")
                }
            }

            Item {
                width: parent.width - 94
                height: parent.height

                Column {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 14

                    TopBar {
                        width: parent.width
                        user: root.user
                        title: root.screen === "home" ? "HOME"
                             : root.screen === "browse" ? "BROWSE"
                             : root.screen === "room" ? "ROOM"
                             : root.screen === "rooms" ? "ROOMS"
                             : root.screen === "controls" ? "CONTROLS"
                             : "SETTINGS"
                        subtitle: root.screen === "home"
                                  ? "Welcome back, " + (root.user.displayName ?? root.user.name ?? "player")
                                  : root.screen === "browse"
                                    ? "Find your next game · " + root.allChannels.length + " channels indexed"
                                    : root.screen === "room"
                                      ? (root.currentChannel?.title ?? root.currentChannel?.name ?? "")
                                      : root.screen === "rooms"
                                        ? root.joinedChannels.length + " joined · " + root.roomLimit() + " slots available"
                                        : root.screen === "controls"
                                          ? "Player 1 bindings for " + (root.currentChannel?.title ?? "this game")
                                          : "Native runtimes, controller, and account"
                    }

                    Item {
                        width: parent.width
                        height: parent.height - 86

                        HomeScreen {
                            id: homeScreen
                            anchors.fill: parent
                            visible: root.screen === "home"
                            channels: root.allChannels
                            lastChannels: settings.lastChannels
                            joinedChannels: root.joinedChannels
                            user: root.user
                            filters: root.filters
                            recentMatches: matchHistory.matches
                            onJoinRequested: function(channelName) { root.joinChannel(channelName, false) }
                            onRecentMatchJoinRequested: function(channelName) { root.joinChannel(channelName, false) }
                            onFavoriteRequested: function(channelName, favorite) { root.toggleFavorite(channelName, favorite) }
                            onBrowseRequested: function() { root.switchScreen("browse") }
                        }

                        BrowseScreen {
                            id: browseScreen
                            anchors.fill: parent
                            visible: root.screen === "browse"
                            channels: root.browseChannels
                            page: root.browsePage
                            totalChannels: root.browseTotal
                            title: root.browseTitle
                            loading: root.browseLoading
                            filters: root.filters
                            joinedChannels: root.joinedChannels
                            user: root.user
                            onSearchRequested: function(request) { root.browseRequest(request) }
                            onJoinRequested: function(channelName) { root.joinChannel(channelName, false) }
                            onFavoriteRequested: function(channelName, favorite) { root.toggleFavorite(channelName, favorite) }
                        }

                        RoomsScreen {
                            id: roomsScreen
                            anchors.fill: parent
                            visible: root.screen === "rooms"
                            rooms: root.joinedChannels
                            currentChannel: root.currentChannel
                            maxChannels: root.roomLimit()
                            unread: root.unread
                            onOpenRequested: function(channelName) { root.openRoom(channelName) }
                            onBrowseRequested: function() { root.switchScreen("browse") }
                        }

                        RoomScreen {
                            id: roomScreen
                            anchors.fill: parent
                            visible: root.screen === "room"
                            channel: root.currentChannel ?? ({})
                            messages: root.currentMessages
                            user: root.user
                            incomingChallenges: root.incomingChallenges
                            outgoingChallenges: root.outgoingChallenges
                            friends: friendsStore.friends
                            launching: root.launching
                            autoMatchActive: root.autoMatchActive
                            favorite: root.currentChannel ? root.isFavorite(root.currentChannel.name) : false
                            onSendChat: function(text) { root.sendChat(text) }
                            onChallengePlayer: function(player) { root.openChallengeForPlayer(player) }
                            onSpectateRequested: function(match) { root.spectateMatch(match) }
                            onLeaveRequested: function() { root.leaveRoom() }
                            onTestRequested: function() { root.startTestGame() }
                            onTrainingRequested: function() { root.startTraining() }
                            onFavoriteRequested: function(favorite) { if (root.currentChannel) root.toggleFavorite(root.currentChannel.name, favorite) }
                            onCopyRequested: function(text) { appController.copyToClipboard(text); root.notify("Copied " + text, "info") }
                            onFriendToggleRequested: function(player) { friendsStore.toggleFriend(player.name ?? "") }
                            onAutoMatchToggleRequested: function() { root.toggleAutoMatch() }
                            onChannelTvRequested: function() { root.startChannelTv() }
                            onAcceptChallenge: function(challenge) { root.openIncomingChallenge(challenge) }
                            onDeclineChallenge: function(challenge) {
                                client.rejectChallenge(challenge.challenger, challenge.channelName, challenge.challengeId)
                                root.incomingChallenges = root.incomingChallenges.filter(function(item) { return item !== challenge })
                            }
                            onCancelChallenge: function(challenge) {
                                client.cancelChallenge(challenge.challenger, challenge.channelName, challenge.challengeId)
                                root.outgoingChallenges = root.outgoingChallenges.filter(function(item) { return item !== challenge })
                            }
                        }

                        ControlsScreen {
                            id: controlsScreen
                            anchors.fill: parent
                            visible: root.screen === "controls"
                            gameId: root.currentChannel?.gameId ?? ""
                            gameTitle: root.currentChannel?.title ?? ""
                            onBackRequested: root.switchScreen("room")
                        }

                        SettingsScreen {
                            id: settingsScreen
                            anchors.fill: parent
                            visible: root.screen === "settings"
                            user: root.user
                            onSignOut: function() {
                                settings.clearSession()
                                client.logout()
                                root.screen = "home"
                            }
                            onClose: function() { root.switchScreen("home") }
                        }
                    }
                }
            }
        }

        ControllerHintBar {
            visible: settings.showControllerHints
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }

        Rectangle {
            id: romProgress
            visible: roms.busy
            width: 430
            height: 64
            radius: 12
            color: "#111722f2"
            border.width: 1
            border.color: theme.cyan
            anchors.right: parent.right
            anchors.rightMargin: 28
            anchors.bottom: parent.bottom
            anchors.bottomMargin: (settings.showControllerHints ? 42 : 0) + 12
            z: 60

            Column {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                Text {
                    width: parent.width
                    text: roms.status
                    color: theme.text
                    elide: Text.ElideRight
                    font.family: theme.bodyFamily
                    font.pixelSize: 11
                }

                Rectangle {
                    width: parent.width
                    height: 6
                    radius: 3
                    color: theme.panelRaised

                    Rectangle {
                        width: parent.width * Math.max(0.02, roms.progress)
                        height: parent.height
                        radius: 3
                        color: theme.cyan
                    }
                }
            }
        }

        Item {
            id: toastLayer
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 82
            anchors.rightMargin: 28
            width: 460
            height: childrenRect.height
            z: 50

            Column {
                width: parent.width
                spacing: 8
                anchors.right: parent.right

                Repeater {
                    model: root.toasts
                    delegate: Toast {
                        required property var modelData
                        width: parent.width
                        message: modelData.message
                        level: modelData.level
                        onExpired: root.removeToast(modelData.id)
                    }
                }
            }
        }

        Rectangle {
            visible: root.launching
            anchors.fill: parent
            color: "#06080dee"
            z: 70

            Column {
                anchors.centerIn: parent
                spacing: 16

                Rectangle {
                    width: 54
                    height: 54
                    radius: 27
                    color: "transparent"
                    border.width: 3
                    border.color: theme.accent
                    anchors.horizontalCenter: parent.horizontalCenter
                    RotationAnimator on rotation {
                        running: root.launching
                        loops: Animation.Infinite
                        from: 0
                        to: 360
                        duration: 900
                    }
                }

                Text {
                    text: "LAUNCHING NATIVE RUNTIME"
                    color: theme.accent
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: 17
                    font.letterSpacing: 2.4
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: root.launchingTitle
                    color: theme.text
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: 36
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "The emulator window will take over in a moment."
                    color: theme.textMuted
                    font.family: theme.bodyFamily
                    font.pixelSize: 13
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }

    ChallengeDialog {
        id: challengeDialog
        anchors.fill: parent
        scale: settings.uiScale
        transformOrigin: Item.Center
        onAccepted: function(ranked) { root.acceptActiveChallenge(ranked) }
        onDeclined: function() { root.declineActiveChallenge() }
        onCancelled: function() { root.cancelChallengeDialog() }
    }

    VirtualKeyboard {
        id: virtualKeyboard
        anchors.fill: parent
        scale: settings.uiScale
        transformOrigin: Item.Center
        title: "Keyboard"
        onClosed: {
            root.virtualKeyboardVisible = false
            focusController.activeGroup = "main"
        }
    }
}
