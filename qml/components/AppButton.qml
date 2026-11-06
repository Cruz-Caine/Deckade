import QtQuick
import QtQuick.Controls

Focusable {
    id: root

    property string text: ""
    property string iconText: ""
    property bool primary: false
    property bool danger: false
    property bool compact: false
    property bool busy: false
    property bool selected: false

    signal clicked()

    implicitWidth: Math.max(compact ? 132 : 168, label.implicitWidth + (iconText.length > 0 ? 66 : 44))
    implicitHeight: compact ? 44 : 54
    ringRadius: 12

    onActivated: {
        if (root.enabled && !root.busy) {
            root.clicked()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: root.primary
               ? (root.activeFocus ? theme.accentSoft : theme.accent)
               : root.danger
                 ? (root.activeFocus ? "#3a1a24" : theme.panel)
                 : (root.activeFocus ? theme.panelHover : root.selected ? theme.panelRaised : theme.panelRaised)
        border.width: 1
        border.color: root.primary
                      ? Qt.lighter(theme.accent, 1.2)
                      : root.danger
                        ? theme.red
                        : root.activeFocus ? theme.borderStrong : theme.border
        Behavior on color {
            ColorAnimation { duration: settings.reduceMotion ? 0 : 120 }
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: 10

        Text {
            visible: root.iconText.length > 0
            text: root.iconText
            color: root.primary ? "#16070a" : theme.text
            font.family: theme.bodyFamily
            font.pixelSize: root.compact ? 17 : 19
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: label
            text: root.text
            color: root.primary ? "#16070a" : root.danger ? theme.red : theme.text
            font.family: theme.displayFamily
            font.weight: Font.DemiBold
            font.pixelSize: root.compact ? 17 : 19
            font.letterSpacing: 0.6
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Rectangle {
        visible: root.busy
        width: 16
        height: 16
        radius: 8
        color: "transparent"
        border.width: 2
        border.color: root.primary ? "#16070a" : theme.accent
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        RotationAnimator on rotation {
            running: root.busy
            loops: Animation.Infinite
            from: 0
            to: 360
            duration: 850
        }
    }

    opacity: enabled ? 1 : 0.4
}

