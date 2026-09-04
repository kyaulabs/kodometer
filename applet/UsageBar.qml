import QtQuick

Item {
    id: root

    property real percent: 0
    property color accentColor: "#49a3b0"
    property real trackOpacity: 0.16
    readonly property real normalizedPercent: Math.max(0, Math.min(100, percent))
    readonly property real fillWidth: width * normalizedPercent / 100

    implicitWidth: 180
    implicitHeight: 6

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Qt.alpha(root.accentColor, root.trackOpacity)
    }

    Rectangle {
        width: root.fillWidth
        height: parent.height
        radius: height / 2
        color: root.accentColor
    }
}
