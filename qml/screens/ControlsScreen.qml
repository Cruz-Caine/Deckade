import QtQuick
import QtQuick.Controls
import "../components"

Item {
    id: screen

    property string gameId: ""
    property string gameTitle: ""
    property string roomEmulator: ""
    property string emulator: "fbneo"

    signal backRequested()

    readonly property bool keyboardMode: settings.gameInputDevice === "keyboard"
    readonly property bool needsRoom: emulator === "fbneo" && gameId.length === 0
    readonly property var keyboardKeys: emulator === "fbneo"
        ? [["Move", "Arrow keys"], ["Button 1", "Z"], ["Button 2", "X"], ["Button 3", "C"], ["Button 4", "V"],
           ["Coin", "5"], ["Start", "1"]]
        : emulator === "snes9x"
          ? [["Move", "Arrow keys"], ["B", "Z"], ["A", "X"], ["Y", "A"], ["X", "S"], ["L", "Q"], ["R", "W"],
             ["Start", "Enter"], ["Select", "Right Shift"]]
          : []

    function normalized(name) {
        if (name === "snes9x" || name === "fcadesnes9x" || name === "snes") return "snes9x"
        if (name === "flycast" || name === "nulldc") return "flycast"
        return "fbneo"
    }

    function focusDefault() {
        Qt.callLater(function() { focusController.focusItem(deviceRepeater.itemAt(0)) })
    }

    function load() {
        controllerProfile.loadFor(screen.emulator, screen.gameId)
    }

    function selectEmulator(name) {
        screen.emulator = name
        load()
    }

    onVisibleChanged: {
        if (!visible) return
        if (roomEmulator.length > 0) screen.emulator = normalized(roomEmulator)
        load()
    }
    onGameIdChanged: if (visible) load()

    Column {
        anchors.fill: parent
        spacing: 14

        Row {
            spacing: 24

            Column {
                spacing: 6
                Text {
                    text: "Play games with"
                    color: theme.textMuted
                    font.family: theme.bodyFamily
                    font.pixelSize: 12
                }
                Row {
                    spacing: 6
                    Repeater {
                        id: deviceRepeater
                        model: [{ label: "Controller", value: "controller" }, { label: "Keyboard", value: "keyboard" }]
                        delegate: AppButton {
                            required property var modelData
                            text: modelData.label
                            compact: true
                            selected: settings.gameInputDevice === modelData.value
                            primary: selected
                            onClicked: settings.gameInputDevice = modelData.value
                        }
                    }
                }
            }

            Column {
                spacing: 6
                Text {
                    text: "Emulator"
                    color: theme.textMuted
                    font.family: theme.bodyFamily
                    font.pixelSize: 12
                }
                Row {
                    spacing: 6
                    Repeater {
                        model: [{ label: "FBNeo", value: "fbneo" }, { label: "Snes9x", value: "snes9x" },
                                { label: "Flycast", value: "flycast" }]
                        delegate: AppButton {
                            required property var modelData
                            text: modelData.label
                            compact: true
                            primary: screen.emulator === modelData.value
                            onClicked: screen.selectEmulator(modelData.value)
                        }
                    }
                }
            }
        }

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            color: theme.textMuted
            font.family: theme.bodyFamily
            font.pixelSize: 13
            text: screen.keyboardMode
                  ? (screen.emulator === "flycast"
                     ? "Flycast uses its built-in keyboard layout."
                     : "These are the keys used in game.")
                  : screen.needsRoom
                    ? "FBNeo buttons differ from game to game. Open a room to set bindings for that game."
                    : screen.emulator === "fbneo"
                      ? "Bindings for " + (screen.gameTitle.length > 0 ? screen.gameTitle : screen.gameId) + ". Saved for this game only."
                      : "Bindings for every " + (screen.emulator === "snes9x" ? "Snes9x" : "Flycast") + " game."
        }

        Rectangle {
            visible: controllerProfile.capturing
            width: parent.width
            height: 44
            radius: 8
            color: theme.panelRaised
            border.width: 2
            border.color: theme.accent
            Text {
                anchors.centerIn: parent
                text: "Press a button or move a stick. B cancels."
                color: theme.text
                font.family: theme.bodyFamily
                font.weight: Font.DemiBold
                font.pixelSize: 14
            }
        }

        Column {
            visible: screen.keyboardMode
            width: parent.width
            spacing: 2
            Repeater {
                model: screen.keyboardKeys
                delegate: Rectangle {
                    required property var modelData
                    width: parent.width
                    height: 40
                    radius: 6
                    color: theme.panel
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData[0]
                        color: theme.text
                        font.family: theme.bodyFamily
                        font.pixelSize: 14
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData[1]
                        color: theme.textMuted
                        font.family: theme.bodyFamily
                        font.weight: Font.DemiBold
                        font.pixelSize: 13
                    }
                }
            }
        }

        ListView {
            id: actionList
            visible: !screen.keyboardMode && !screen.needsRoom
            width: parent.width
            height: parent.height - 196
            clip: true
            spacing: 2
            model: controllerProfile.actions
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Focusable {
                id: actionRow
                required property var modelData
                width: actionList.width
                height: 42
                ringOutside: false
                ringRadius: 6
                hintPrimary: "Rebind"
                hintSecondary: modelData.kind !== "" ? "Clear" : ""
                onActivated: controllerProfile.prepareCapture(modelData.index)
                onSecondaryActivated: controllerProfile.clearBinding(modelData.index)

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: actionRow.hovered ? theme.panelRaised : theme.panel
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 240
                    text: modelData.title
                    color: theme.text
                    font.family: theme.bodyFamily
                    font.weight: Font.Medium
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.kind === "" ? "Default" : modelData.binding
                    color: modelData.kind === "" ? theme.textFaint : theme.accentSoft
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 13
                }
            }
        }

        Row {
            visible: !screen.keyboardMode && !screen.needsRoom
            spacing: 12

            AppButton {
                text: "Reset to defaults"
                compact: true
                danger: true
                onClicked: controllerProfile.resetDefaults()
            }

            Text {
                text: controllerProfile.status
                color: theme.textFaint
                font.family: theme.bodyFamily
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
