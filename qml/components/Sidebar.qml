import QtQuick

Rectangle {
    id: root

    property int currentIndex: 0
    property int roomCount: 0
    signal selected(int index)

    width: 94
    color: "#0b0e16cc"
    border.width: 0

    Column {
        anchors.fill: parent
        anchors.topMargin: 18
        anchors.bottomMargin: 14
        spacing: 8

        Item {
            width: parent.width
            height: 52

            Image {
                anchors.centerIn: parent
                width: 42
                height: 42
                source: "qrc:/assets/icons/deckade.svg"
                smooth: true
            }
        }

        Item { width: 1; height: 12 }

        Repeater {
            model: [
                { icon: "⌂", label: "HOME" },
                { icon: "⌕", label: "BROWSE" },
                { icon: "▤", label: "ROOMS" },
                { icon: "⚙", label: "SETTINGS" }
            ]

            delegate: Focusable {
                id: navItem
                required property int index
                required property var modelData
                property bool current: root.currentIndex === index

                width: 94
                height: 68
                navGroup: "main"
                ringRadius: 12
                onActivated: root.selected(index)

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 9
                    radius: 12
                    color: navItem.current ? theme.panelRaised : navItem.activeFocus ? theme.panelHover : "transparent"
                    border.width: navItem.current ? 1 : 0
                    border.color: theme.borderStrong
                }

                Rectangle {
                    visible: navItem.current
                    width: 3
                    height: 28
                    radius: 2
                    color: theme.accent
                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 1

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.icon
                        color: navItem.current ? theme.accent : theme.textMuted
                        font.family: theme.bodyFamily
                        font.pixelSize: 24
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.label
                        color: navItem.current ? theme.text : theme.textFaint
                        font.family: theme.displayFamily
                        font.weight: Font.DemiBold
                        font.pixelSize: 9
                        font.letterSpacing: 0.9
                    }
                }

                Rectangle {
                    visible: index === 2 && root.roomCount > 0 && !navItem.current
                    width: 7
                    height: 7
                    radius: 4
                    color: theme.cyan
                    anchors.top: parent.top
                    anchors.topMargin: 15
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        Item { width: 1; height: 6 }

        Item {
            width: parent.width
            height: 54

            Column {
                anchors.centerIn: parent
                spacing: 4

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: gamepad.connected ? theme.green : theme.textFaint
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: gamepad.connected ? "PAD" : "KEYS"
                    color: theme.textFaint
                    font.family: theme.displayFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 9
                    font.letterSpacing: 1.2
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }
}
