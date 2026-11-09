import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property string currentScreen: "home"
    property string currentRoom: ""
    property var rooms: []
    property var unread: ({})
    property int roomLimit: 5

    // The model is just the room names: `rooms` is replaced on every player
    // update, and rebuilding the rows each time would drop the selection.
    property var roomNames: []
    onRoomsChanged: {
        var next = (rooms ?? []).map(function(room) { return room.name })
        if (next.join("\n") !== roomNames.join("\n")) roomNames = next
    }
    function roomByName(name) {
        return (rooms ?? []).find(function(room) { return room.name === name }) ?? ({})
    }

    signal selected(string screen)
    signal roomOpened(string name)
    signal roomLeft(string name)

    color: theme.backgroundAlt

    component NavRow: Focusable {
        id: row
        property string label: ""
        property string detail: ""
        property bool current: false
        property color detailColor: theme.textFaint
        width: parent.width
        height: 40
        ringOutside: false
        ringRadius: 6

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: row.current ? theme.panelRaised : row.hovered ? theme.panel : "transparent"
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: detailText.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: row.label
            color: row.current ? theme.text : theme.textMuted
            elide: Text.ElideRight
            font.family: theme.bodyFamily
            font.weight: row.current ? Font.DemiBold : Font.Medium
            font.pixelSize: 14
        }

        Text {
            id: detailText
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: row.detail
            color: row.detailColor
            font.family: theme.bodyFamily
            font.pixelSize: 12
        }
    }

    Column {
        id: topColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 2

        Row {
            height: 54
            spacing: 10
            leftPadding: 6

            Image {
                width: 26
                height: 26
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/assets/icons/deckade.svg"
                smooth: true
            }

            Text {
                text: "Deckade"
                color: theme.text
                anchors.verticalCenter: parent.verticalCenter
                font.family: theme.bodyFamily
                font.weight: Font.Bold
                font.pixelSize: 17
            }
        }

        // Controls are per game, so they live in each room's header instead
        // of taking a permanent sidebar slot.
        Repeater {
            model: [
                { label: "Home", screen: "home" },
                { label: "Browse", screen: "browse" },
                { label: "Settings", screen: "settings" }
            ]

            delegate: NavRow {
                required property var modelData
                label: modelData.label
                current: root.currentScreen === modelData.screen
                hintPrimary: "Open"
                onActivated: root.selected(modelData.screen)
            }
        }

        Item { width: 1; height: 12 }

        NavRow {
            label: "Rooms"
            detail: root.rooms.length + " of " + root.roomLimit
            current: root.currentScreen === "rooms"
            hintPrimary: "Manage rooms"
            onActivated: root.selected("rooms")
        }
    }

    ListView {
        id: roomList
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topColumn.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.topMargin: 2
        anchors.bottomMargin: 10
        clip: true
        spacing: 2
        model: root.roomNames
        boundsBehavior: Flickable.StopAtBounds

        delegate: NavRow {
            required property string modelData
            readonly property var room: root.roomByName(modelData)
            width: roomList.width
            label: room.title ?? modelData
            current: root.currentScreen === "room" && root.currentRoom === modelData
            detail: (root.unread[modelData] ?? 0) > 0
                    ? String(root.unread[modelData])
                    : String(room.playerCount ?? (room.users ?? []).length)
            detailColor: (root.unread[modelData] ?? 0) > 0 ? theme.accentSoft : theme.textFaint
            hintPrimary: "Open room"
            hintSecondary: "Leave room"
            onActivated: root.roomOpened(modelData)
            onSecondaryActivated: root.roomLeft(modelData)
        }
    }
}
