import QtQuick
import org.kde.kirigami as Kirigami

Item {
    id: root

    property real percent: 0
    property color accentColor: "#49a3b0"
    property real trackOpacity: 0.16
    readonly property real normalizedPercent: Number.isFinite(percent) ? Math.max(0, Math.min(100,
                                                                                              percent)) :
                                                                         0
    readonly property real fillWidth: width * normalizedPercent / 100

    implicitWidth: 180
    implicitHeight: 10

    Rectangle {
        x: root.fillWidth > 0 ? root.fillWidth + 2 : 0
        width: Math.max(0, root.width - x)
        height: root.height
        radius: 2
        color: Qt.alpha(Kirigami.Theme.textColor, root.trackOpacity)
    }

    Rectangle {
        width: root.fillWidth
        height: parent.height
        radius: 2
        color: root.accentColor
    }
}
