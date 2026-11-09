import QtQuick
import QtQuick.Controls
import "../components"

Item {
    id: screen

    property var rooms: []
    property var currentChannel: null
    property int maxChannels: 5
    property var unread: ({})

    signal openRequested(string channelName)
    signal browseRequested()

    function columns() {
        if (width >= 1500) return 4
        if (width >= 1050) return 3
        return 2
    }

    function focusDefault() {
        Qt.callLater(function() {
            if (roomRepeater.count > 0) {
                focusController.focusItem(roomRepeater.itemAt(0))
            } else {
                browseButton.forceActiveFocus(Qt.OtherFocusReason)
                focusController.focusItem(browseButton)
            }
        })
    }

    Column {
        anchors.fill: parent
        spacing: 16

        Row {
            width: parent.width
            height: 56

            Column {
                width: parent.width - 260
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Text {
                    text: "Your rooms"
                    color: theme.text
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: 24
                }

                Text {
                    text: screen.rooms.length + " joined  ·  " + screen.maxChannels + " slots on this account"
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 12
                }
            }

            AppButton {
                id: browseButton
                text: "Browse more"
                compact: true
                width: 210
                anchors.verticalCenter: parent.verticalCenter
                onClicked: screen.browseRequested()
            }
        }

        Flickable {
            id: flick
            width: parent.width
            height: parent.height - 100
            visible: screen.rooms.length > 0
            contentWidth: width
            contentHeight: roomGrid.height + 16
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            Grid {
                id: roomGrid
                width: parent.width
                columns: screen.columns()
                columnSpacing: 14
                rowSpacing: 14

                Repeater {
                    id: roomRepeater
                    model: screen.rooms

                    delegate: GameCard {
                        required property var modelData
                        width: (roomGrid.width - (roomGrid.columns - 1) * 14) / roomGrid.columns
                        height: width * 0.58
                        channel: modelData
                        joined: true
                        favorite: false
                        onJoinRequested: screen.openRequested(modelData.name)
                    }
                }
            }
        }

        Rectangle {
            visible: screen.rooms.length === 0
            width: parent.width
            height: visible ? 220 : 0
            radius: 10
            color: theme.panel
            border.width: 1
            border.color: theme.border

            Column {
                anchors.centerIn: parent
                spacing: 10

                Text {
                    text: "No rooms joined"
                    color: theme.text
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: 25
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "Join a game channel and it will stay here until you leave it."
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 13
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                AppButton {
                    text: "Browse games"
                    compact: true
                    primary: true
                    width: 190
                    anchors.horizontalCenter: parent.horizontalCenter
                    onClicked: screen.browseRequested()
                }
            }
        }

        Text {
            visible: screen.rooms.length > 0
            text: "A opens a room  ·  B returns home  ·  leave a room from its header"
            color: theme.textFaint
            font.family: theme.bodyFamily
            font.pixelSize: 11
        }
    }
}
