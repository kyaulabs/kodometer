pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    property real sessionRemaining: 0
    property real weeklyRemaining: 0
    property color accentColor: "#49a3b0"
    readonly property real sessionFillWidth: width * Math.max(0, Math.min(100, sessionRemaining))
                                             / 100
    readonly property real weeklyFillWidth: width * Math.max(0, Math.min(100, weeklyRemaining))
                                            / 100

    implicitWidth: 22
    implicitHeight: 18

    Column {
        anchors.fill: parent
        spacing: 2

        Repeater {
            model: [root.sessionFillWidth, root.weeklyFillWidth]

            Item {
                required property real modelData
                width: root.width
                height: (root.height - 2) / 2

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: Qt.alpha(root.accentColor, 0.18)
                }

                Rectangle {
                    width: parent.modelData
                    height: parent.height
                    radius: height / 2
                    color: root.accentColor
                }
            }
        }
    }
}
