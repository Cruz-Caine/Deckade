import QtQuick
import QtQuick.Controls
import "../components"

Item {
    id: screen

    property string gameId: ""
    property string gameTitle: ""

    signal backRequested()

    function focusDefault() {
        Qt.callLater(function() {
            if (actionList.count > 0) {
                focusController.focusItem(actionList.itemAtIndex(0))
            } else {
                backButton.forceActiveFocus(Qt.OtherFocusReason)
                focusController.focusItem(backButton)
            }
        })
    }

    function load() {
        controllerProfile.loadForGame(gameId)
        focusDefault()
    }

    Column {
        anchors.fill: parent
        spacing: 14

        Row {
            width: parent.width
            height: 58

            Column {
                width: parent.width - 280
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Text {
                    text: "CONTROLLER BINDINGS"
                    color: theme.text
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: 24
                    font.letterSpacing: 2
                }

                Text {
                    width: parent.width
                    text: (screen.gameTitle.length > 0 ? screen.gameTitle + " · " : "")
                          + controllerProfile.controllerName
                          + (controllerProfile.controllerIndex >= 0 ? " (PAD " + (controllerProfile.controllerIndex + 1) + ")" : "")
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            AppButton {
                id: backButton
                text: "BACK TO ROOM"
                compact: true
                width: 210
                anchors.verticalCenter: parent.verticalCenter
                onClicked: screen.backRequested()
            }
        }

        Rectangle {
            visible: controllerProfile.capturing
            width: parent.width
            height: 54
            radius: 10
            color: "#1a1014"
            border.width: 1
            border.color: theme.accent

            Text {
                anchors.centerIn: parent
                text: "PRESS A BUTTON, D-PAD DIRECTION, OR MOVE A STICK  ·  START/MENU TO CANCEL"
                color: theme.accent
                font.family: theme.displayFamily
                font.weight: Font.DemiBold
                font.pixelSize: 13
                font.letterSpacing: 1.2
            }
        }

        Text {
            width: parent.width
            text: controllerProfile.status
            color: theme.textFaint
            font.family: theme.bodyFamily
            font.pixelSize: 11
        }

        ListView {
            id: actionList
            width: parent.width
            height: parent.height - 190
            clip: true
            spacing: 6
            model: controllerProfile.actions
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Focusable {
                id: actionRow
                required property var modelData
                width: actionList.width
                height: 48
                ringRadius: 9
                onActivated: controllerProfile.prepareCapture(modelData.index)
                onSecondaryActivated: controllerProfile.clearBinding(modelData.index)

                Rectangle {
                    anchors.fill: parent
                    radius: 9
                    color: actionRow.activeFocus ? theme.panelHover : theme.panel
                    border.width: 1
                    border.color: actionRow.activeFocus ? theme.accent : theme.border
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 300
                    text: modelData.title
                    color: theme.text
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.binding
                    color: modelData.kind === "" ? theme.textFaint : theme.cyan
                    font.family: theme.displayFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 12
                }
            }
        }

        Row {
            width: parent.width
            spacing: 10

            AppButton {
                text: "RESET TO FBNEO DEFAULTS"
                compact: true
                width: 270
                danger: true
                onClicked: controllerProfile.resetDefaults()
            }

            Text {
                width: parent.width - 300
                text: "A binds the highlighted action. X clears it. Unbound actions keep FBNeo's keyboard/default controller bindings."
                color: theme.textFaint
                font.family: theme.bodyFamily
                font.pixelSize: 10
                wrapMode: Text.WordWrap
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    onVisibleChanged: {
        if (visible && gameId.length > 0) load()
    }
}
