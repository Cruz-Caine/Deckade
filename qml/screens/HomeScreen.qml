import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: screen

    property var channels: []
    property var lastChannels: []
    property var joinedChannels: []
    property var user: ({})
    property var filters: ({})
    property string userName: ""
    property var recentMatches: []

    signal joinRequested(string channelName)
    signal recentMatchJoinRequested(string channelName)
    signal favoriteRequested(string channelName, bool favorite)
    signal joinToggleRequested(string channelName)
    signal browseRequested()

    function featuredChannel() {
        if (!channels || channels.length === 0) return null
        var sorted = channels.slice().sort(function(a, b) { return (b.playerCount ?? 0) - (a.playerCount ?? 0) })
        return sorted[0]
    }

    function popularChannels() {
        if (!channels) return []
        return channels.slice().sort(function(a, b) {
            return (b.playerCount ?? 0) - (a.playerCount ?? 0)
        }).slice(0, 8)
    }

    function isJoined(name) {
        return (joinedChannels ?? []).some(function(channel) { return channel.name === name })
    }

    function isFavorite(name) {
        return ((user.favoritesChannels ?? [])).indexOf(name) !== -1
    }

    function focusDefault() {
        if (hero) {
            hero.forceActiveFocus(Qt.OtherFocusReason)
            focusController.focusItem(hero)
        }
    }

    function recentRoomData() {
        var result = []
        var known = channels ?? []
        for (var i = 0; i < (lastChannels ?? []).length; ++i) {
            var name = lastChannels[i]
            var channel = known.find(function(c) { return c.name === name })
            if (channel) result.push(channel)
        }
        return result
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.height + 18
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn
            width: flick.width
            spacing: 20

            Row {
                width: parent.width
                height: 252
                spacing: 16

                Focusable {
                    id: hero
                    width: parent.width - 356
                    height: parent.height
                    ringRadius: 18
                    hintPrimary: "Join and open"
                    hintSecondary: "Join"
                    onSecondaryActivated: {
                        var featured = screen.featuredChannel()
                        if (featured) screen.joinToggleRequested(featured.name)
                    }
                    onActivated: {
                        var featured = screen.featuredChannel()
                        if (featured) screen.joinRequested(featured.name)
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: theme.panel
                        clip: true
                        border.width: 1
                        border.color: hero.activeFocus ? theme.accent : theme.border

                        Image {
                            anchors.fill: parent
                            source: screen.featuredChannel()?.previewUrl ?? ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                            opacity: 0.85
                        }

                        Rectangle {
                            anchors.fill: parent
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#090c13aa" }
                                GradientStop { position: 0.4; color: "#090c1355" }
                                GradientStop { position: 1.0; color: "#090c13f4" }
                            }
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 24
                            anchors.rightMargin: 24
                            anchors.bottomMargin: 22
                            spacing: 8

                            Row {
                                spacing: 8
                                Pill {
                                    text: "Featured"
                                    strong: true
                                    foreground: theme.accent
                                    line: "#5a2430"
                                    fill: "#1a0d12"
                                }
                                Pill {
                                    text: String(screen.featuredChannel()?.playerCount ?? 0) + " online"
                                    dot: true
                                    foreground: theme.green
                                    line: "#245544"
                                    fill: "#0b1512"
                                }
                                Pill {
                                    visible: screen.featuredChannel()?.isRanked === true
                                    text: "Ranked"
                                    strong: true
                                    foreground: theme.gold
                                    line: "#5b4720"
                                    fill: "#1a160b"
                                }
                            }

                            Text {
                                width: parent.width
                                text: screen.featuredChannel()?.title ?? "No channels yet"
                                color: theme.text
                                font.family: theme.displayFamily
                                font.weight: Font.Bold
                                font.pixelSize: 30
                                elide: Text.ElideRight
                            }

                            Row {
                                spacing: 12
                                Text {
                                    text: screen.featuredChannel()?.system ?? ""
                                    color: theme.textMuted
                                    font.family: theme.bodyFamily
                                    font.pixelSize: 13
                                }
                                Text {
                                    text: screen.featuredChannel()?.gameId ?? ""
                                    color: theme.textFaint
                                    font.family: theme.bodyFamily
                                    font.pixelSize: 13
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: 340
                    height: parent.height
                    radius: 10
                    color: theme.panel
                    border.width: 1
                    border.color: theme.border

                    Column {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 10

                        Text {
                            text: "Jump back in"
                            color: theme.textMuted
                            font.family: theme.displayFamily
                            font.weight: Font.DemiBold
                            font.pixelSize: 13
                        }

                        Text {
                            visible: screen.recentRoomData().length === 0
                            width: parent.width
                            text: "Rooms you join will show up here for quick access."
                            color: theme.textFaint
                            font.family: theme.bodyFamily
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Column {
                            visible: screen.recentRoomData().length > 0
                            width: parent.width
                            spacing: 6

                            Repeater {
                                model: screen.recentRoomData()

                                delegate: Focusable {
                                    id: recentItem
                                    required property var modelData
                                    width: parent.width
                                    height: 48
                                    ringRadius: 9
                                    onActivated: screen.joinRequested(modelData.name)

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 9
                                        color: recentItem.activeFocus ? theme.panelHover : theme.panelRaised
                                        border.width: 1
                                        border.color: recentItem.activeFocus ? theme.accent : "transparent"
                                    }

                                    Row {
                                        anchors.fill: parent
                                        anchors.margins: 9
                                        spacing: 10

                                        Rectangle {
                                            width: 30
                                            height: 30
                                            radius: 7
                                            color: "#0b1512"
                                            border.width: 1
                                            border.color: "#245544"
                                            Text {
                                                anchors.centerIn: parent
                                                text: "▶"
                                                color: theme.green
                                                font.pixelSize: 11
                                            }
                                        }

                                        Column {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: parent.width - 62
                                            Text {
                                                text: modelData.title ?? modelData.name
                                                color: theme.text
                                                elide: Text.ElideRight
                                                font.family: theme.displayFamily
                                                font.weight: Font.DemiBold
                                                font.pixelSize: 15
                                                width: parent.width
                                            }
                                            Text {
                                                text: (modelData.playerCount ?? 0) + " online"
                                                color: theme.textFaint
                                                font.family: theme.bodyFamily
                                                font.pixelSize: 10
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Item { width: 1; height: 1 }

                        AppButton {
                            width: parent.width
                            text: "Browse all games"
                            compact: true
                            onClicked: screen.browseRequested()
                        }
                    }
                }
            }

            Row {
                width: parent.width
                height: 26

                Text {
                    text: "Popular now"
                    color: theme.text
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: 19
                }

                Item { width: parent.width - 280; height: 1 }

                Text {
                    text: "Live channels"
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Grid {
                id: popularGrid
                width: parent.width
                columns: width >= 1500 ? 5 : width >= 1000 ? 4 : 3
                columnSpacing: 14
                rowSpacing: 14

                Repeater {
                    model: screen.popularChannels()

                    delegate: GameCard {
                        required property var modelData
                        width: (popularGrid.width - (popularGrid.columns - 1) * 14) / popularGrid.columns
                        height: Math.max(width * 0.62, 176)
                        channel: modelData
                        joined: screen.isJoined(modelData.name)
                        favorite: screen.isFavorite(modelData.name)
                        onJoinRequested: screen.joinRequested(modelData.name)
                        onJoinToggleRequested: screen.joinToggleRequested(modelData.name)
                        onFavoriteRequested: screen.favoriteRequested(modelData.name, !screen.isFavorite(modelData.name))
                    }
                }
            }

            Row {
                width: parent.width
                height: 26
                Text {
                    text: "Recent matches"
                    color: theme.text
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: 16
                }
                Item { width: parent.width - 320; height: 1 }
                Text {
                    text: ""
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 10
                }
            }

            Column {
                width: parent.width
                spacing: 6
                visible: screen.recentMatches.length > 0
                Repeater {
                    model: screen.recentMatches.slice(0, 5)
                    delegate: Focusable {
                        id: recentMatch
                        required property var modelData
                        width: parent.width
                        height: 46
                        ringRadius: 8
                        onActivated: screen.recentMatchJoinRequested(modelData.channelName ?? "")
                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            color: recentMatch.activeFocus ? theme.panelHover : theme.panel
                            border.width: 1
                            border.color: recentMatch.activeFocus ? theme.accent : theme.border
                        }
                        Row {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 12
                            Text {
                                text: "↩"
                                color: theme.cyan
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                width: parent.width - 220
                                text: (modelData.opponent ?? "") + "  ·  " + (modelData.channelName ?? "")
                                color: theme.text
                                elide: Text.ElideRight
                                font.family: theme.bodyFamily
                                font.pixelSize: 13
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData.ranked > 0 ? ("FT" + modelData.ranked) : "Casual"
                                color: theme.textFaint
                                font.family: theme.bodyFamily
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: screen.popularChannels().length === 0
                width: parent.width
                height: 120
                radius: 10
                color: theme.panel
                border.width: 1
                border.color: theme.border
                Text {
                    anchors.centerIn: parent
                    text: "Loading Fightcade channels..."
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 14
                }
            }
        }
    }
}
