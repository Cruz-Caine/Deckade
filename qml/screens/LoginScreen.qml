import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: screen

    property bool restoring: false
    property string errorText: ""
    property string statusText: ""
    property string busyText: "SIGNING IN"
    property bool rememberLogin: settings.rememberLogin
    signal submit(string username, string password)

    function submitForm() {
        if (usernameField.text.trim().length === 0 || passwordField.text.length === 0) {
            errorText = "Enter your Fightcade username and password."
            return
        }
        errorText = ""
        screen.submit(usernameField.text.trim(), passwordField.text)
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#07090f" }
            GradientStop { position: 0.62; color: "#0c111b" }
            GradientStop { position: 1.0; color: "#141b2a" }
        }
    }

    Rectangle {
        width: 620
        height: 620
        radius: 310
        x: -220
        y: -260
        color: "#19ff4655"
        opacity: 0.55
    }

    Rectangle {
        width: 520
        height: 520
        radius: 260
        anchors.right: parent.right
        anchors.rightMargin: -180
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -210
        color: "#163dd6ff"
        opacity: 0.6
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Math.max(30, Math.min(72, screen.width * 0.045))
        spacing: Math.max(24, Math.min(64, screen.width * 0.035))

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 22

                Image {
                    source: "qrc:/assets/icons/deckade.svg"
                    width: 92
                    height: 92
                    smooth: true
                }

                Text {
                    text: "DECKADE"
                    color: theme.text
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: Math.max(52, Math.min(86, screen.width * 0.062))
                    font.letterSpacing: 6
                }

                Rectangle {
                    width: 92
                    height: 5
                    radius: 3
                    color: theme.accent
                }

                Text {
                    width: parent.width
                    text: "FIGHTCADE, REBUILT FOR THE COUCH."
                    color: theme.textMuted
                    font.family: theme.displayFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: Math.max(18, Math.min(22, screen.width * 0.017))
                    font.letterSpacing: 2.5
                    wrapMode: Text.WordWrap
                }

                Text {
                    width: parent.width
                    text: "Native rollback runtimes. Big-picture navigation. No Wine, no browser, no keyboard required."
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 15
                    lineHeight: 1.35
                    wrapMode: Text.WordWrap
                }

                Row {
                    spacing: 10

                    Pill { text: "NATIVE GGPO"; strong: true; foreground: theme.green; line: "#245544"; fill: "#0d1915" }
                    Pill { text: "CONTROLLER FIRST"; strong: true; foreground: theme.cyan; line: "#24516b"; fill: "#0e1c27" }
                    Pill { text: "STEAM DECK"; strong: true; foreground: theme.gold; line: "#5b4720"; fill: "#1a160b" }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: Math.min(432, Math.max(380, screen.width * 0.42))
            Layout.preferredHeight: 560
            radius: 20
            color: "#111722ee"
            border.width: 1
            border.color: theme.borderStrong

            Column {
                anchors.fill: parent
                anchors.margins: 30
                spacing: 18

                Text {
                    text: "SIGN IN"
                    color: theme.accent
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: 20
                    font.letterSpacing: 2.6
                }

                Text {
                    width: parent.width
                    text: "Use your Fightcade account. Deckade never stores your password."
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Item { width: 1; height: 8 }

                Text {
                    text: "USERNAME"
                    color: theme.textMuted
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 10
                    font.letterSpacing: 1.5
                }

                DeckField {
                    id: usernameField
                    width: parent.width
                    placeholderText: "Fightcade username"
                    enabled: !screen.restoring
                    Keys.onReturnPressed: screen.submitForm()
                    Keys.onTabPressed: passwordField.forceActiveFocus()
                }

                Text {
                    text: "PASSWORD"
                    color: theme.textMuted
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 10
                    font.letterSpacing: 1.5
                }

                DeckField {
                    id: passwordField
                    width: parent.width
                    placeholderText: "Password"
                    echoMode: TextInput.Password
                    enabled: !screen.restoring
                    Keys.onReturnPressed: screen.submitForm()
                }

                Text {
                    visible: screen.errorText.length > 0
                    width: parent.width
                    text: screen.errorText
                    color: theme.red
                    font.family: theme.bodyFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Item { width: 1; height: 2 }

                Focusable {
                    width: parent.width
                    height: 30
                    navGroup: "main"
                    navEnabled: !screen.restoring
                    ringRadius: 7
                    onActivated: {
                        screen.rememberLogin = !screen.rememberLogin
                        settings.rememberLogin = screen.rememberLogin
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18
                        height: 18
                        radius: 4
                        color: screen.rememberLogin ? theme.accent : theme.panel
                        border.width: 1
                        border.color: screen.rememberLogin ? theme.accent : theme.borderStrong
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 30
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Remember me on this device"
                        color: theme.textMuted
                        font.family: theme.bodyFamily
                        font.pixelSize: 12
                    }
                }

                AppButton {
                    width: parent.width
                    text: screen.restoring ? screen.busyText : "ENTER THE ARCADE"
                    primary: true
                    busy: screen.restoring
                    enabled: !screen.restoring
                    onClicked: screen.submitForm()
                }

                Text {
                    visible: screen.statusText.length > 0
                    width: parent.width
                    text: screen.statusText
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { width: 1; height: 12 }

                Text {
                    width: parent.width
                    text: "A select   ·   Y on-screen keyboard   ·   Steam + X also works"
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 10
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Component.onCompleted: {
        if (settings.hasSession && screen.restoring) {
            usernameField.text = settings.sessionName
        }
        Qt.callLater(function() {
            if (usernameField.text.length === 0) {
                usernameField.forceActiveFocus()
            } else {
                passwordField.forceActiveFocus()
            }
        })
    }

    onVisibleChanged: {
        if (visible) {
            Qt.callLater(function() {
                if (usernameField.text.length === 0) usernameField.forceActiveFocus()
                else passwordField.forceActiveFocus()
            })
        }
    }
}
