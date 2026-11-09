import QtQuick

Rectangle {
    id: root

    property string inputMode: "controller"
    property var item: null

    readonly property bool keyboard: inputMode === "keyboard"
    readonly property bool textField: item !== null && item.deckTextInput === true
    readonly property var hints: {
        const list = []
        const add = function(pad, key, label) {
            if (label && label.length > 0) list.push({ button: keyboard ? key : pad, label: label })
        }
        if (textField) {
            add("A", "Enter", keyboard ? "Confirm" : "Open keyboard")
        } else {
            add("A", "Enter", item ? item.hintPrimary : "Select")
            add("X", "X", item ? item.hintSecondary : "")
            add("Y", "F", item ? item.hintTertiary : "")
        }
        add("B", "Esc", "Back")
        add("Lb / rb", "Q / e", "Switch page")
        add("Menu", "M", "Settings")
        return list
    }

    height: 42
    color: theme.backgroundAlt

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        spacing: 20

        Repeater {
            model: root.hints
            delegate: Row {
                required property var modelData
                spacing: 7
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    width: Math.max(24, badgeText.implicitWidth + 12)
                    height: 22
                    radius: 5
                    color: theme.panelHover
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: modelData.button
                        color: theme.text
                        font.family: theme.bodyFamily
                        font.weight: Font.DemiBold
                        font.pixelSize: 11
                    }
                }

                Text {
                    text: modelData.label
                    color: theme.textMuted
                    font.family: theme.bodyFamily
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        text: gamepad.connected ? gamepad.controllerName : "No controller connected"
        color: theme.textFaint
        font.family: theme.bodyFamily
        font.pixelSize: 12
        elide: Text.ElideRight
        width: 220
        horizontalAlignment: Text.AlignRight
    }
}
