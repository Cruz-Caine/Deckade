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

    implicitWidth: Math.max(compact ? 96 : 140, label.implicitWidth + (iconText.length > 0 ? 56 : 36))
    implicitHeight: compact ? 40 : 48
    hintPrimary: text
    onActivated: {
        if (root.enabled && !root.busy) {
            root.clicked()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: root.primary ? (root.hovered ? theme.accentSoft : theme.accent)
             : root.selected ? theme.panelHover
             : root.hovered ? theme.panelHover : theme.panelRaised
    }

    Row {
        anchors.centerIn: parent
        spacing: 8

        Text {
            visible: root.iconText.length > 0
            text: root.iconText
            color: label.color
            font.family: theme.bodyFamily
            font.pixelSize: 15
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: label
            text: root.text
            color: root.primary ? "#ffffff" : root.danger ? theme.red : theme.text
            font.family: theme.bodyFamily
            font.weight: Font.DemiBold
            font.pixelSize: root.compact ? 13 : 15
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Rectangle {
        visible: root.busy
        width: 14
        height: 14
        radius: 7
        color: "transparent"
        border.width: 2
        border.color: root.primary ? "#ffffff" : theme.accent
        anchors.right: parent.right
        anchors.rightMargin: 14
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
