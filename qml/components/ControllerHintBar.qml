import QtQuick

Rectangle {
    id: root

    property var hints: [
        { button: "A", label: "Select" },
        { button: "B", label: "Back" },
        { button: "X", label: "Favorite" },
        { button: "Y", label: "Keyboard" },
        { button: "LB/RB", label: "Section" },
        { button: "☰", label: "Menu" }
    ]

    height: 42
    color: "#0a0d14dd"
    border.width: 0

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.verticalCenter: parent.verticalCenter
        spacing: 18

        Repeater {
            model: root.hints
            delegate: Row {
                spacing: 7
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    width: Math.max(24, badgeText.implicitWidth + 12)
                    height: 22
                    radius: 6
                    color: theme.panelHover
                    border.width: 1
                    border.color: theme.borderStrong

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: modelData.button
                        color: theme.text
                        font.family: theme.displayFamily
                        font.weight: Font.DemiBold
                        font.pixelSize: 11
                    }
                }

                Text {
                    text: modelData.label
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 22
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Rectangle {
            width: 7
            height: 7
            radius: 4
            color: gamepad.connected ? theme.green : theme.textFaint
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: gamepad.connected ? gamepad.controllerName : "Keyboard navigation"
            color: theme.textFaint
            font.family: theme.bodyFamily
            font.pixelSize: 11
            elide: Text.ElideRight
            width: 250
            horizontalAlignment: Text.AlignRight
        }
    }
}
