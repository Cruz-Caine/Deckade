import QtQuick
import QtQuick.Controls
import "."

FocusScope {
    id: root

    property var target: null
    property bool shift: false
    property string title: "Keyboard"
    signal closed()

    visible: false
    focus: visible
    z: 90

    function open(targetItem) {
        target = targetItem
        shift = false
        visible = true
        forceActiveFocus()
        focusController.activeGroup = "keyboard"
        Qt.callLater(focusController.focusFirst)
    }

    function close() {
        const previous = target
        visible = false
        target = null
        focusController.activeGroup = "main"
        if (previous) {
            previous.forceActiveFocus(Qt.OtherFocusReason)
            focusController.focusItem(previous)
        }
        closed()
    }

    function append(text) {
        if (!target) return
        const current = target.text ?? ""
        target.text = current + text
        if (target.cursorPosition !== undefined) {
            target.cursorPosition = target.text.length
        }
    }

    function backspace() {
        if (!target) return
        const current = target.text ?? ""
        if (current.length > 0) {
            target.text = current.slice(0, -1)
            if (target.cursorPosition !== undefined) {
                target.cursorPosition = target.text.length
            }
        }
    }

    function press(key) {
        if (key === "shift") {
            shift = !shift
            return
        }
        if (key === "space") {
            append(" ")
            return
        }
        if (key === "backspace") {
            backspace()
            return
        }
        if (key === "done") {
            close()
            return
        }
        append(shift ? key.toUpperCase() : key.toLowerCase())
        shift = false
    }

    Rectangle {
        anchors.fill: parent
        color: "#06080dcc"

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    Rectangle {
        id: panel
        width: Math.min(820, root.width - 80)
        height: 430
        anchors.centerIn: parent
        radius: 18
        color: theme.backgroundAlt
        border.width: 1
        border.color: theme.borderStrong

        MouseArea {
            anchors.fill: parent
        }

        Column {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            Row {
                width: parent.width
                height: 30

                Text {
                    text: root.title.toUpperCase()
                    color: theme.accent
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: 17
                    font.letterSpacing: 2.2
                }

                Item { width: parent.width - 360; height: 1 }

                Text {
                    text: root.target && root.target.echoMode === TextInput.Password
                          ? "•".repeat(Math.min(24, (root.target.text ?? "").length))
                          : (root.target?.text ?? "")
                    width: 300
                    elide: Text.ElideLeft
                    horizontalAlignment: Text.AlignRight
                    color: theme.text
                    font.family: theme.bodyFamily
                    font.pixelSize: 15
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Repeater {
                model: ["1234567890", "qwertyuiop", "asdfghjkl;", "zxcvbnm,./"]

                delegate: Row {
                    required property string modelData
                    property string rowText: modelData
                    spacing: 6
                    anchors.horizontalCenter: parent.horizontalCenter

                    Repeater {
                        model: rowText.length

                        delegate: Focusable {
                            id: keyDelegate
                            required property int index
                            property string key: rowText.charAt(index)

                            width: 64
                            height: 56
                            navGroup: "keyboard"
                            ringRadius: 9
                            onActivated: root.press(key)

                            Rectangle {
                                anchors.fill: parent
                                radius: 9
                                color: parent.activeFocus ? theme.panelHover : theme.panelRaised
                                border.width: 1
                                border.color: parent.activeFocus ? theme.accent : theme.border
                            }

                            Text {
                                anchors.centerIn: parent
                                text: root.shift ? keyDelegate.key.toUpperCase() : keyDelegate.key
                                color: theme.text
                                font.family: theme.bodyFamily
                                font.weight: Font.DemiBold
                                font.pixelSize: 18
                            }
                        }
                    }
                }
            }

            Row {
                spacing: 8
                anchors.horizontalCenter: parent.horizontalCenter

                Focusable {
                    width: 110
                    height: 54
                    navGroup: "keyboard"
                    ringRadius: 9
                    onActivated: root.press("shift")

                    Rectangle {
                        anchors.fill: parent
                        radius: 9
                        color: parent.activeFocus ? theme.panelHover : root.shift ? "#33241a" : theme.panelRaised
                        border.width: 1
                        border.color: parent.activeFocus ? theme.accent : root.shift ? theme.gold : theme.border
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "SHIFT"
                        color: root.shift ? theme.gold : theme.textMuted
                        font.family: theme.displayFamily
                        font.weight: Font.DemiBold
                        font.pixelSize: 12
                        font.letterSpacing: 1.4
                    }
                }

                Focusable {
                    width: 260
                    height: 54
                    navGroup: "keyboard"
                    ringRadius: 9
                    onActivated: root.press("space")

                    Rectangle {
                        anchors.fill: parent
                        radius: 9
                        color: parent.activeFocus ? theme.panelHover : theme.panelRaised
                        border.width: 1
                        border.color: parent.activeFocus ? theme.accent : theme.border
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "SPACE"
                        color: theme.textMuted
                        font.family: theme.displayFamily
                        font.weight: Font.DemiBold
                        font.pixelSize: 12
                        font.letterSpacing: 1.4
                    }
                }

                Focusable {
                    width: 140
                    height: 54
                    navGroup: "keyboard"
                    ringRadius: 9
                    onActivated: root.press("backspace")

                    Rectangle {
                        anchors.fill: parent
                        radius: 9
                        color: parent.activeFocus ? theme.panelHover : theme.panelRaised
                        border.width: 1
                        border.color: parent.activeFocus ? theme.accent : theme.border
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "⌫ DELETE"
                        color: theme.textMuted
                        font.family: theme.displayFamily
                        font.weight: Font.DemiBold
                        font.pixelSize: 12
                        font.letterSpacing: 1.1
                    }
                }

                Focusable {
                    width: 110
                    height: 54
                    navGroup: "keyboard"
                    ringRadius: 9
                    onActivated: root.close()

                    Rectangle {
                        anchors.fill: parent
                        radius: 9
                        color: parent.activeFocus ? theme.accentSoft : theme.accent
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "DONE"
                        color: "#16070a"
                        font.family: theme.displayFamily
                        font.weight: Font.Bold
                        font.pixelSize: 13
                        font.letterSpacing: 1.4
                    }
                }
            }
        }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.close()
            event.accepted = true
        } else if (event.key === Qt.Key_Backspace) {
            root.backspace()
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.close()
            event.accepted = true
        } else if (event.text.length === 1) {
            root.append(event.text)
            event.accepted = true
        }
    }
}
