import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: screen

    property bool restoring: false
    property string restoringName: ""
    property string errorText: ""
    property string statusText: ""
    property string busyText: "Signing in"
    property bool rememberLogin: settings.rememberLogin
    signal submit(string username, string password)
    signal cancelRestore()

    function submitForm() {
        if (usernameField.text.trim().length === 0 || passwordField.text.length === 0) {
            errorText = "Enter your Fightcade username and password."
            return
        }
        errorText = ""
        screen.submit(usernameField.text.trim(), passwordField.text)
    }

    function focusForm() {
        Qt.callLater(function() {
            if (usernameField.text.length === 0) usernameField.forceActiveFocus()
            else passwordField.forceActiveFocus()
        })
    }

    Rectangle {
        anchors.fill: parent
        color: theme.background
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Math.max(24, Math.min(72, screen.width * 0.045))
        spacing: Math.max(24, Math.min(64, screen.width * 0.035))

        // The branding block needs real width. On a narrow window the sign-in
        // card takes the whole screen instead of squashing the wordmark.
        Item {
            visible: screen.width >= 900
            Layout.fillWidth: true
            Layout.fillHeight: true

            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                spacing: 22

                Image {
                    source: "qrc:/assets/icons/deckade.svg"
                    width: 92
                    height: 92
                    smooth: true
                }

                Text {
                    text: "Deckade"
                    color: theme.text
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: Math.max(52, Math.min(86, screen.width * 0.062))
                }

                Rectangle {
                    width: 92
                    height: 5
                    radius: 3
                    color: theme.accent
                }

                Text {
                    width: parent.width
                    text: "Fightcade, rebuilt for the couch."
                    color: theme.textMuted
                    font.family: theme.displayFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: Math.max(18, Math.min(22, screen.width * 0.017))
                    wrapMode: Text.WordWrap
                }

                Text {
                    width: Math.min(parent.width, 520)
                    text: "Native rollback runtimes. Big-picture navigation. No Wine, no browser, no keyboard required."
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 15
                    lineHeight: 1.35
                    wrapMode: Text.WordWrap
                }

                Row {
                    spacing: 10

                    Pill { text: "Native GGPO"; strong: true; foreground: theme.green; line: "#245544"; fill: "#0d1915" }
                    Pill { text: "Controller first"; strong: true; foreground: theme.cyan; line: "#24516b"; fill: "#0e1c27" }
                    Pill { text: "Steam deck"; strong: true; foreground: theme.gold; line: "#5b4720"; fill: "#1a160b" }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: Math.min(432, Math.max(320, screen.width * 0.42))
            Layout.preferredHeight: 560
            Layout.alignment: Qt.AlignVCenter
            radius: 10
            color: theme.panel
            border.width: 1
            border.color: theme.border

            // Restoring a remembered session is its own screen, not a login
            // form with dead fields. The user waits on a clear progress card.
            Column {
                visible: screen.restoring
                anchors.fill: parent
                anchors.margins: 30
                spacing: 18

                Item { width: 1; height: 24 }

                Text {
                    text: "Welcome back"
                    color: theme.accent
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: 26
                }

                Text {
                    width: parent.width
                    text: screen.restoringName.length > 0
                          ? "Signing you in as " + screen.restoringName
                          : "Restoring your Fightcade session"
                    color: theme.text
                    font.family: theme.bodyFamily
                    font.pixelSize: 15
                    wrapMode: Text.WordWrap
                }

                Row {
                    spacing: 12

                    Rectangle {
                        width: 18
                        height: 18
                        radius: 9
                        color: "transparent"
                        border.width: 2
                        border.color: theme.accent
                        anchors.verticalCenter: parent.verticalCenter
                        RotationAnimator on rotation {
                            running: screen.restoring
                            loops: Animation.Infinite
                            from: 0
                            to: 360
                            duration: 850
                        }
                    }

                    Text {
                        text: screen.busyText
                        color: theme.textMuted
                        font.family: theme.bodyFamily
                        font.pixelSize: 14
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    visible: screen.statusText.length > 0
                    width: parent.width
                    text: screen.statusText
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
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

                Item { width: 1; height: 40 }

                Text {
                    width: parent.width
                    text: "Deckade never stores your password. The saved Fightcade session stays on this device until you sign out."
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 11
                    lineHeight: 1.35
                    wrapMode: Text.WordWrap
                }

                AppButton {
                    width: parent.width
                    text: "Use another account"
                    compact: true
                    onClicked: screen.cancelRestore()
                }
            }

            Column {
                visible: !screen.restoring
                anchors.fill: parent
                anchors.margins: 30
                spacing: 18

                Text {
                    text: "Sign in"
                    color: theme.accent
                    font.family: theme.displayFamily
                    font.weight: Font.Bold
                    font.pixelSize: 20
                }

                Text {
                    width: parent.width
                    text: "Use your Fightcade account. Deckade never stores your password."
                    color: theme.textFaint
                    font.family: theme.bodyFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
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

                Item { width: 1; height: 4 }

                Text {
                    text: "Username"
                    color: theme.textMuted
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 10
                }

                DeckField {
                    id: usernameField
                    width: parent.width
                    placeholderText: "Fightcade username"
                    Keys.onReturnPressed: screen.submitForm()
                    Keys.onTabPressed: passwordField.forceActiveFocus()
                }

                Text {
                    text: "Password"
                    color: theme.textMuted
                    font.family: theme.bodyFamily
                    font.weight: Font.DemiBold
                    font.pixelSize: 10
                }

                DeckField {
                    id: passwordField
                    width: parent.width
                    placeholderText: "Password"
                    echoMode: TextInput.Password
                    Keys.onReturnPressed: screen.submitForm()
                }

                Item { width: 1; height: 2 }

                Focusable {
                    width: parent.width
                    height: 30
                    navGroup: "main"
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
                    text: "Sign in"
                    primary: true
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

                Text {
                    width: parent.width
                    text: "Environment login: set DECKADE_USERNAME and DECKADE_PASSWORD."
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
        if (settings.hasSession) {
            usernameField.text = settings.sessionName
        }
        if (!screen.restoring) screen.focusForm()
    }

    onRestoringChanged: {
        if (!screen.restoring) {
            screen.focusForm()
        }
    }

    onVisibleChanged: {
        if (visible && !screen.restoring) screen.focusForm()
    }
}
