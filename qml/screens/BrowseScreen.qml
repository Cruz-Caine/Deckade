import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: screen

    property var channels: []
    property int page: 0
    property int totalChannels: 0
    property string title: "ALL GAMES"
    property bool loading: false
    property var filters: ({})
    property var joinedChannels: []
    property var user: ({})
    property int filterMode: 0 // 0 all, 1 ranked, 2 unranked, 3 favorites

    signal searchRequested(var request)
    signal joinRequested(string channelName)
    signal favoriteRequested(string channelName, bool favorite)

    function requestSearch(targetPage) {
        var request = {
            text: searchField.text,
            page: targetPage ?? 0
        }
        if (screen.filterMode === 1) request.ranked = true
        if (screen.filterMode === 2) request.ranked = false
        if (screen.filterMode === 3) request.favorites = true
        screen.searchRequested(request)
    }

    function totalPages() {
        return Math.max(1, Math.ceil(screen.totalChannels / 50))
    }

    function isJoined(name) {
        return (screen.joinedChannels ?? []).some(function(channel) { return channel.name === name })
    }

    function isFavorite(name) {
        return ((screen.user.favoritesChannels ?? [])).indexOf(name) !== -1
    }

    function focusSearch() {
        if (searchField) {
            searchField.forceActiveFocus(Qt.OtherFocusReason)
            focusController.focusItem(searchField)
        }
    }

    Column {
        anchors.fill: parent
        spacing: 14

        Row {
            width: parent.width
            height: 56
            spacing: 12

            Text {
                text: screen.title.toUpperCase()
                color: theme.text
                font.family: theme.displayFamily
                font.weight: Font.Bold
                font.pixelSize: 24
                font.letterSpacing: 2
                width: 300
                elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }

            DeckField {
                id: searchField
                width: parent.width - 616
                height: 48
                placeholderText: "Search games, systems, or ROM names"
                anchors.verticalCenter: parent.verticalCenter
                onTextChanged: searchDebounce.restart()
                onAccepted: screen.requestSearch(0)
            }

            AppButton {
                text: "SEARCH"
                compact: true
                width: 126
                anchors.verticalCenter: parent.verticalCenter
                onClicked: screen.requestSearch(0)
            }

            Item { width: parent.width - 1048; height: 1 }
        }

        Row {
            width: parent.width
            height: 34
            spacing: 8

            Repeater {
                model: [
                    { label: "ALL", mode: 0 },
                    { label: "RANKED", mode: 1 },
                    { label: "UNRANKED", mode: 2 },
                    { label: "FAVORITES", mode: 3 }
                ]

                delegate: Focusable {
                    id: filterItem
                    required property var modelData
                    width: 116
                    height: 32
                    ringRadius: 8
                    onActivated: {
                        screen.filterMode = modelData.mode
                        screen.requestSearch(0)
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        color: screen.filterMode === modelData.mode
                               ? theme.accent
                               : filterItem.activeFocus ? theme.panelHover : theme.panel
                        border.width: 1
                        border.color: screen.filterMode === modelData.mode ? theme.accent : theme.border
                    }
                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        color: screen.filterMode === modelData.mode ? "#16070a" : theme.textMuted
                        font.family: theme.displayFamily
                        font.weight: Font.DemiBold
                        font.pixelSize: 11
                        font.letterSpacing: 1.1
                    }
                }
            }

            Item { width: 1; height: 1 }

        }

        Flickable {
            id: flick
            width: parent.width
            height: parent.height - 120
            contentWidth: width
            contentHeight: grid.height + 92
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            Item {
                width: flick.width
                height: grid.height + 92

                Grid {
                    id: grid
                    width: parent.width
                    columns: width >= 1500 ? 5 : width >= 1050 ? 4 : 3
                    columnSpacing: 14
                    rowSpacing: 14

                    Repeater {
                        model: screen.channels

                        delegate: GameCard {
                            required property var modelData
                            width: (grid.width - (grid.columns - 1) * 14) / grid.columns
                            height: width * 0.58
                            channel: modelData
                            joined: screen.isJoined(modelData.name)
                            favorite: screen.isFavorite(modelData.name)
                            onJoinRequested: screen.joinRequested(modelData.name)
                            onFavoriteRequested: screen.favoriteRequested(modelData.name, !screen.isFavorite(modelData.name))
                        }
                    }
                }

                Rectangle {
                    visible: screen.loading
                    anchors.centerIn: parent
                    width: 260
                    height: 90
                    radius: 14
                    color: theme.panel
                    border.width: 1
                    border.color: theme.border
                    Text {
                        anchors.centerIn: parent
                        text: "Searching Fightcade..."
                        color: theme.textMuted
                        font.family: theme.bodyFamily
                        font.pixelSize: 13
                    }
                }

                Text {
                    visible: !screen.loading && screen.channels.length === 0
                    anchors.centerIn: parent
                    text: "No channels matched that search."
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 14
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: grid.bottom
                    anchors.topMargin: 16
                    spacing: 12

                    AppButton {
                        text: "‹ PREVIOUS"
                        compact: true
                        enabled: screen.page > 0
                        onClicked: screen.requestSearch(Math.max(0, screen.page - 1))
                    }

                    Rectangle {
                        width: 138
                        height: 44
                        radius: 10
                        color: theme.panel
                        border.width: 1
                        border.color: theme.border
                        Text {
                            anchors.centerIn: parent
                            text: "PAGE " + (screen.page + 1) + " / " + screen.totalPages()
                            color: theme.textMuted
                            font.family: theme.displayFamily
                            font.weight: Font.DemiBold
                            font.pixelSize: 11
                            font.letterSpacing: 1
                        }
                    }

                    AppButton {
                        text: "NEXT ›"
                        compact: true
                        enabled: screen.page + 1 < screen.totalPages()
                        onClicked: screen.requestSearch(screen.page + 1)
                    }
                }
            }
        }
    }

    Timer {
        id: searchDebounce
        interval: 450
        onTriggered: screen.requestSearch(0)
    }

    onVisibleChanged: {
        if (visible) {
            if (screen.channels.length === 0 && !screen.loading) {
                Qt.callLater(function() { screen.requestSearch(0) })
            }
            Qt.callLater(function() {
                if (!searchField.activeFocus) searchField.forceActiveFocus()
                focusController.focusFirst()
            })
        }
    }
}
