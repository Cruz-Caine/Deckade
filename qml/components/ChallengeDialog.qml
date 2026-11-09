import QtQuick
import QtQuick.Controls
import "."

FocusScope {
    id: root

    property bool incoming: false
    property var player: ({})
    property var challenge: ({})
    property int defaultRanked: 3
    signal accepted(int ranked)
    signal declined()
    signal cancelled()

    visible: false
    focus: visible
    z: 80

    function open(isIncoming, targetPlayer, incomingChallenge, ranked) {
        incoming = isIncoming
        player = targetPlayer ?? ({})
        challenge = incomingChallenge ?? ({})
        defaultRanked = ranked ?? 3
        visible = true
        forceActiveFocus()
        focusController.activeGroup = "modal"
        Qt.callLater(focusController.focusFirst)
    }

    function close() {
        visible = false
        focusController.activeGroup = "main"
    }

    function accept(ranked) {
        accepted(ranked)
        close()
    }

    function decline() {
        declined()
        close()
    }

    function cancel() {
        cancelled()
        close()
    }

    Rectangle {
        anchors.fill: parent
        color: "#06080dcc"
    }

    Rectangle {
        id: panel
        width: Math.min(700, Math.max(360, root.width - 48))
        height: root.incoming ? 360 : 430
        anchors.centerIn: parent
        radius: 10
        color: theme.backgroundAlt
        border.width: 1
        border.color: theme.borderStrong

        Column {
            anchors.fill: parent
            anchors.margins: 26
            spacing: 18

            Text {
                text: root.incoming ? "Incoming challenge" : "Challenge player"
                color: root.incoming ? theme.gold : theme.accent
                font.family: theme.displayFamily
                font.weight: Font.Bold
                font.pixelSize: 17
            }

            Row {
                width: parent.width
                spacing: 18

                Avatar {
                    size: 70
                    hash: root.player.gravatar ?? ""
                    name: root.player.name ?? ""
                    online: !(root.player.away === true)
                    anchors.verticalCenter: parent.verticalCenter
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 5
                    width: parent.width - 100

                    Text {
                        text: root.player.name ?? "Unknown player"
                        color: theme.text
                        font.family: theme.displayFamily
                        font.weight: Font.Bold
                        font.pixelSize: 30
                        width: parent.width
                        elide: Text.ElideRight
                    }

                    Text {
                        visible: (root.challenge.channelTitle ?? "").length > 0
                        width: parent.width
                        text: (root.incoming ? "from #" : "in #")
                              + (root.challenge.channelName ?? "")
                              + (root.challenge.channelTitle !== root.challenge.channelName
                                 ? "  ·  " + root.challenge.channelTitle : "")
                        color: theme.cyan
                        font.family: theme.bodyFamily
                        font.weight: Font.DemiBold
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Text {
                        text: root.incoming
                              ? "wants to play " + (root.challenge.ranked > 0 ? "FT" + root.challenge.ranked : "a casual set")
                              : "Choose a match length. Ranked channels use the selected FT."
                        color: theme.textMuted
                        font.family: theme.bodyFamily
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                }
            }

            Grid {
                id: rankGrid
                visible: !root.incoming
                width: parent.width
                columns: 3
                spacing: 10
                anchors.horizontalCenter: parent.horizontalCenter

                Repeater {
                    model: [
                        { label: "FT2", value: 2 },
                        { label: "FT3", value: 3 },
                        { label: "FT5", value: 5 },
                        { label: "FT10", value: 10 },
                        { label: "FT20", value: 20 },
                        { label: "Unranked", value: 0 }
                    ]

                    delegate: AppButton {
                        required property var modelData
                        text: modelData.label
                        compact: true
                        primary: modelData.value === root.defaultRanked
                        width: Math.max(110, (rankGrid.width - 20) / 3)
                        navGroup: "modal"
                        onClicked: root.accept(modelData.value)
                    }
                }
            }

            Row {
                visible: !root.incoming
                spacing: 12
                anchors.horizontalCenter: parent.horizontalCenter

                AppButton {
                    text: "Cancel"
                    compact: true
                    navGroup: "modal"
                    onClicked: root.cancel()
                }
            }

            Row {
                visible: root.incoming
                spacing: 12
                anchors.horizontalCenter: parent.horizontalCenter

                AppButton {
                    text: "Decline"
                    compact: true
                    danger: true
                    navGroup: "modal"
                    onClicked: root.decline()
                }

                AppButton {
                    text: root.challenge.ranked > 0 ? "Accept FT" + root.challenge.ranked : "Accept"
                    compact: true
                    primary: true
                    navGroup: "modal"
                    onClicked: root.accept(root.challenge.ranked ?? 0)
                }
            }
        }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.incoming ? root.decline() : root.cancel()
            event.accepted = true
        }
    }
}
