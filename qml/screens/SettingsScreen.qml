import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import "../components"

Item {
    id: screen

    property var user: ({})

    signal signOut()
    signal close()

    function localPath(url) {
        return decodeURIComponent(url.toString().replace("file://", ""))
    }

    function focusDefault() {
        focusController.focusFirst()
    }

    function cycleScale() {
        const values = [0.8, 0.9, 1.0]
        let index = 0
        for (let i = 0; i < values.length; ++i) {
            if (Math.abs(values[i] - settings.uiScale) < 0.01) { index = i; break }
        }
        settings.uiScale = values[(index + 1) % values.length]
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.height + 12
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        Column {
            id: content
            width: flick.width
            spacing: 14

            Rectangle {
                width: parent.width
                height: 150
                radius: 10
                color: theme.panel
                border.width: 1
                border.color: theme.border

                Column {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    Text {
                        text: "Game files"
                        color: theme.text
                        font.family: theme.displayFamily
                        font.weight: Font.Bold
                        font.pixelSize: 17
                    }

                    Text {
                        text: launcher.romPath.length > 0 ? "ROMs: " + launcher.romPath : "ROM folder not found yet."
                        color: launcher.romPath.length > 0 ? theme.green : theme.gold
                        font.family: theme.bodyFamily
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                        width: parent.width
                    }

                    Row {
                        spacing: 8

                        AppButton {
                            text: "Choose folder"
                            compact: true
                            width: 158
                            onClicked: folderDialog.open()
                        }

                        AppButton {
                            text: "Open ROMs"
                            compact: true
                            width: 126
                            onClicked: launcher.openRomFolder()
                        }

                        AppButton {
                            text: "Refresh"
                            compact: true
                            width: 108
                            onClicked: launcher.refreshInstall()
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 178
                radius: 10
                color: theme.panel
                border.width: 1
                border.color: theme.border

                Column {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 8

                    Text {
                        text: "Native runtimes"
                        color: theme.text
                        font.family: theme.displayFamily
                        font.weight: Font.Bold
                        font.pixelSize: 17
                    }

                    Text {
                        text: "Fightcade emulators run as native processes. No Wine, no browser."
                        color: theme.textFaint
                        font.family: theme.bodyFamily
                        font.pixelSize: 11
                    }

                    Row {
                        width: parent.width
                        height: 92
                        spacing: 10

                        Repeater {
                            model: launcher.runtimeSummary

                            delegate: Rectangle {
                                required property var modelData
                                width: (parent.width - 30) / 4
                                height: 92
                                radius: 10
                                color: modelData.available ? "#0d1915" : "#16131a"
                                border.width: 1
                                border.color: modelData.available ? "#245544" : theme.border

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 3

                                    Text {
                                        text: modelData.name
                                        color: modelData.available ? theme.green : theme.textMuted
                                        font.family: theme.displayFamily
                                        font.weight: Font.Bold
                                        font.pixelSize: 15
                                    }
                                    Text {
                                        text: modelData.available ? "NATIVE · READY" : "Not installed"
                                        color: modelData.available ? theme.green : theme.gold
                                        font.family: theme.bodyFamily
                                        font.weight: Font.DemiBold
                                        font.pixelSize: 9
                                    }
                                    Text {
                                        width: parent.width
                                        text: (modelData.systems ?? []).slice(0, 2).join(" · ")
                                        color: theme.textFaint
                                        font.family: theme.bodyFamily
                                        font.pixelSize: 9
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Row {
                width: parent.width
                height: 272
                spacing: 14

                Rectangle {
                    width: (parent.width - 14) / 2
                    height: parent.height
                    radius: 10
                    color: theme.panel
                    border.width: 1
                    border.color: theme.border

                    Column {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 10

                        Text {
                            text: "Controller"
                            color: theme.text
                            font.family: theme.displayFamily
                            font.weight: Font.Bold
                            font.pixelSize: 17
                        }

                        Text {
                            text: gamepad.connected ? gamepad.controllerName : "No controller detected"
                            color: gamepad.connected ? theme.green : theme.gold
                            font.family: theme.bodyFamily
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            width: parent.width
                        }

                        Text {
                            width: parent.width
                            text: "D-pad / left stick navigate · A select · B back · X favorite · Y keyboard · LB/RB section · Start menu"
                            color: theme.textFaint
                            font.family: theme.bodyFamily
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            width: parent.width
                            text: "Steam Deck in Game Mode presents the built-in controls as an Xbox-style controller. Bluetooth pads work the same way."
                            color: theme.textFaint
                            font.family: theme.bodyFamily
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }

                        Focusable {
                            width: parent.width
                            height: 38
                            navGroup: "main"
                            ringRadius: 8
                            onActivated: {
                                const names = gamepad.controllerNames()
                                if (names.length > 1) {
                                    let next = (gamepad.controllerIndex + 1) % names.length
                                    if (next < 0) next = 0
                                    gamepad.selectController(next)
                                    settings.controllerIndex = next
                                }
                            }

                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Player 1 controller"
                                color: theme.textMuted
                                font.family: theme.bodyFamily
                                font.pixelSize: 12
                                width: parent.width - 220
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: gamepad.controllerIndex >= 0
                                      ? "pad " + (gamepad.controllerIndex + 1)
                                      : "No pad"
                                color: gamepad.connected ? theme.cyan : theme.textFaint
                                font.family: theme.displayFamily
                                font.weight: Font.DemiBold
                                font.pixelSize: 12
                            }
                        }

                        Text {
                            width: parent.width
                            text: "A cycles the controller used for both the launcher and Player 1 in netplay."
                            color: theme.textFaint
                            font.family: theme.bodyFamily
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Rectangle {
                    width: (parent.width - 14) / 2
                    height: parent.height
                    radius: 10
                    color: theme.panel
                    border.width: 1
                    border.color: theme.border

                    Column {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 8

                        Text {
                            text: "Interface"
                            color: theme.text
                            font.family: theme.displayFamily
                            font.weight: Font.Bold
                            font.pixelSize: 17
                        }

                        Focusable {
                            width: parent.width
                            height: 38
                            navGroup: "main"
                            ringRadius: 8
                            onActivated: settings.fullscreen = !settings.fullscreen

                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Fullscreen"
                                color: theme.textMuted
                                font.family: theme.bodyFamily
                                font.pixelSize: 12
                                width: parent.width - 70
                            }
                            Toggle {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                checked: settings.fullscreen
                            }
                        }

                        Focusable {
                            width: parent.width
                            height: 38
                            navGroup: "main"
                            ringRadius: 8
                            onActivated: screen.cycleScale()

                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: "UI scale (shrink to fit)"
                                color: theme.textMuted
                                font.family: theme.bodyFamily
                                font.pixelSize: 12
                                width: parent.width - 90
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: Math.round(settings.uiScale * 100) + "%"
                                color: theme.cyan
                                font.family: theme.displayFamily
                                font.weight: Font.DemiBold
                                font.pixelSize: 13
                            }
                        }

                        Focusable {
                            width: parent.width
                            height: 38
                            navGroup: "main"
                            ringRadius: 8
                            onActivated: settings.showControllerHints = !settings.showControllerHints

                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Controller hints"
                                color: theme.textMuted
                                font.family: theme.bodyFamily
                                font.pixelSize: 12
                                width: parent.width - 70
                            }
                            Toggle {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                checked: settings.showControllerHints
                            }
                        }

                        Focusable {
                            width: parent.width
                            height: 38
                            navGroup: "main"
                            ringRadius: 8
                            onActivated: settings.autoJoinLastChannels = !settings.autoJoinLastChannels

                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Rejoin last rooms"
                                color: theme.textMuted
                                font.family: theme.bodyFamily
                                font.pixelSize: 12
                                width: parent.width - 70
                            }
                            Toggle {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                checked: settings.autoJoinLastChannels
                            }
                        }

                        Focusable {
                            width: parent.width
                            height: 38
                            navGroup: "main"
                            ringRadius: 8
                            onActivated: settings.reduceMotion = !settings.reduceMotion

                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Reduce motion"
                                color: theme.textMuted
                                font.family: theme.bodyFamily
                                font.pixelSize: 12
                                width: parent.width - 70
                            }
                            Toggle {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                checked: settings.reduceMotion
                            }
                        }

                        Focusable {
                            width: parent.width
                            height: 38
                            navGroup: "main"
                            ringRadius: 8
                            onActivated: settings.autoDownloadRoms = !settings.autoDownloadRoms

                            Text {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Auto-download ROMs"
                                color: theme.textMuted
                                font.family: theme.bodyFamily
                                font.pixelSize: 12
                                width: parent.width - 70
                            }
                            Toggle {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                checked: settings.autoDownloadRoms
                            }
                        }

                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 140
                radius: 10
                color: theme.panel
                border.width: 1
                border.color: theme.border

                Row {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 20

                    Avatar {
                        size: 60
                        hash: screen.user.gravatar ?? ""
                        name: screen.user.displayName ?? screen.user.name ?? ""
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Column {
                        width: parent.width - 360
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4
                        Text {
                            text: screen.user.displayName ?? screen.user.name ?? ""
                            color: theme.text
                            font.family: theme.displayFamily
                            font.weight: Font.Bold
                            font.pixelSize: 24
                        }
                        Text {
                            text: "@" + (screen.user.name ?? "") + "  ·  Fightcade account"
                            color: theme.textFaint
                            font.family: theme.bodyFamily
                            font.pixelSize: 12
                        }
                        Text {
                            text: "Deckade is independent and not affiliated with Fightcade. No ROMs are included."
                            color: theme.textFaint
                            font.family: theme.bodyFamily
                            font.pixelSize: 10
                            width: parent.width
                            wrapMode: Text.WordWrap
                        }
                    }

                    Column {
                        width: 260
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8

                        AppButton {
                            width: parent.width
                            text: "Sign out"
                            danger: true
                            compact: true
                            onClicked: screen.signOut()
                        }

                        Text {
                            width: parent.width
                            text: "Deckade " + appController.version
                            color: theme.textFaint
                            font.family: theme.bodyFamily
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        visible: flick.contentHeight > flick.height + 1
        width: 5
        radius: 2.5
        color: theme.borderStrong
        height: Math.max(26, flick.height * flick.height / flick.contentHeight)
        x: flick.x + flick.width - 8
        y: flick.y + (flick.contentY / Math.max(1, flick.contentHeight - flick.height))
                 * (flick.height - height)
    }

    FolderDialog {
        id: folderDialog
        title: "Select your ROM folder"
        onAccepted: {
            settings.romPath = screen.localPath(selectedFolder)
            launcher.refreshInstall()
        }
    }

    component Toggle: Rectangle {
        id: toggle
        property bool checked: false
        width: 42
        height: 22
        radius: 11
        color: toggle.checked ? theme.accent : theme.panelRaised
        border.width: 1
        border.color: toggle.checked ? theme.accent : theme.border

        Rectangle {
            width: 16
            height: 16
            radius: 8
            color: "#16070a"
            anchors.verticalCenter: parent.verticalCenter
            x: toggle.checked ? toggle.width - width - 4 : 4
            Behavior on x { NumberAnimation { duration: settings.reduceMotion ? 0 : 120 } }
        }
    }
}
