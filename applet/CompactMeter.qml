pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.core as PlasmaCore

Item {
    id: root

    property real sessionRemaining: NaN
    property real weeklyRemaining: NaN
    property bool donutCharts: false
    property bool vertical: false
    property bool systemAccent: false
    property color accentColor: palette.accentColor
    property string sessionColor
    property string weeklyColor
    property var providers: []
    property bool toolTipsEnabled: true
    property int toolTipLocation: PlasmaCore.Types.Floating
    readonly property real sessionFillWidth: width * session.value / 100
    readonly property real weeklyFillWidth: width * weekly.value / 100
    readonly property real ringSpacing: 4
    readonly property real ringSpan: ((vertical ? height : width) - ringSpacing) / 2
    readonly property real ringSize: Math.max(0, Math.min(vertical ? width : height, ringSpan))
    readonly property string quotaDescription: qsTr("Session: %1; Weekly: %2").arg(
                                                   session.remainingText).arg(weekly.remainingText)

    implicitWidth: donutCharts ? (vertical ? 48 : 100) : 22
    implicitHeight: donutCharts ? (vertical ? 100 : 48) : 18

    BrandPalette {
        id: palette
        systemAccent: root.systemAccent
    }

    Column {
        anchors.fill: parent
        spacing: 2
        visible: !root.donutCharts

        Repeater {
            model: [root.sessionFillWidth, root.weeklyFillWidth]

            Item {
                required property real modelData
                width: root.width
                height: Math.max(0, (root.height - 2) / 2)

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: Qt.alpha(root.accentColor, 0.22)
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

    QuotaRing {
        id: session
        objectName: "sessionRing"
        visible: root.donutCharts
        width: root.ringSize
        height: width
        x: root.vertical ? (root.width - width) / 2 : (root.width - 2 * width - root.ringSpacing)
                           / 2
        y: root.vertical ? (root.height - 2 * height - root.ringSpacing) / 2 : (root.height
                                                                                - height) / 2
        remaining: root.sessionRemaining
        accentColor: /^#[0-9a-fA-F]{6}$/.test(root.sessionColor) ? root.sessionColor :
                                                                   root.accentColor
        quotaLabel: qsTr("Session quota")
        PlasmaCore.ToolTipArea {
            objectName: "sessionToolTip"
            anchors.fill: parent
            active: root.donutCharts && root.toolTipsEnabled
            mainText: sessionTip.mainText
            subText: sessionTip.subText
            textFormat: Text.PlainText
            location: root.toolTipLocation
        }
        PanelToolTip {
            id: sessionTip
            providers: root.providers
            donutCharts: true
            hoveredQuota: "session"
        }
    }

    QuotaRing {
        id: weekly
        objectName: "weeklyRing"
        visible: root.donutCharts
        width: root.ringSize
        height: width
        x: root.vertical ? session.x : session.x + width + root.ringSpacing
        y: root.vertical ? session.y + height + root.ringSpacing : session.y
        remaining: root.weeklyRemaining
        accentColor: /^#[0-9a-fA-F]{6}$/.test(root.weeklyColor) ? root.weeklyColor :
                                                                  root.accentColor
        quotaLabel: qsTr("Weekly quota")
        PlasmaCore.ToolTipArea {
            objectName: "weeklyToolTip"
            anchors.fill: parent
            active: root.donutCharts && root.toolTipsEnabled
            mainText: weeklyTip.mainText
            subText: weeklyTip.subText
            textFormat: Text.PlainText
            location: root.toolTipLocation
        }
        PanelToolTip {
            id: weeklyTip
            providers: root.providers
            donutCharts: true
            hoveredQuota: "weekly"
        }
    }
}
