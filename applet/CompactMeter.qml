pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami
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
    readonly property real sessionFillWidth: width * normalized(sessionRemaining) / 100
    readonly property real weeklyFillWidth: width * normalized(weeklyRemaining) / 100
    readonly property int providerCount: quotaPresentation.quotaProviders.length
    readonly property real ringSpacing: 4
    readonly property real ringSpan: ((vertical ? height : width) - Math.max(0, providerCount - 1)
                                      * ringSpacing) / Math.max(1, providerCount)
    readonly property real ringSize: Math.max(0, Math.min(vertical ? width : height, ringSpan))
    readonly property string quotaDescription: {
        if (!donutCharts)
            return qsTr("Session: %1; Weekly: %2").arg(remainingText(sessionRemaining)).arg(
                        remainingText(weeklyRemaining))
        return quotaPresentation.quotaProviders.map(provider => String(provider.name
                                                                       || provider.id) + ": "
                                                                + provider.windows.map(windowData
                                                                                       => String(
                                                                                              windowData.label
                                                                                              || windowData.kind)
                                                                                          + " " + remainingText(
                                                                                              windowData.remainingPercent)).join(
                                                                    ", ")).join("; ") || qsTr(
                    "No quota reported")
    }

    function remainingText(value) {
        return Number.isFinite(value) ? qsTr("%1% remaining").arg(normalized(value).toFixed(1)) :
                                        qsTr("Not reported")
    }

    function normalized(value) {
        return Number.isFinite(value) ? Math.max(0, Math.min(100, value)) : 0
    }

    implicitWidth: donutCharts ? (vertical ? 48 : Math.max(1, providerCount) * 52 - 4) : 22
    implicitHeight: donutCharts ? (vertical ? Math.max(1, providerCount) * 52 - 4 : 48) : 18

    BrandPalette {
        id: palette
        systemAccent: root.systemAccent
    }

    QuotaPresentation {
        id: quotaPresentation
        providers: root.providers
        // The applet passes its already-filtered presentation snapshot.
        showSpark: true
        showIdleWindows: true
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

    Kirigami.Icon {
        objectName: "emptyQuotaMeter"
        anchors.centerIn: parent
        width: Math.min(root.width, root.height)
        height: width
        visible: root.donutCharts && root.providerCount === 0
        source: Qt.resolvedUrl("assets/kodometer-symbolic.svg")
        isMask: true
    }

    Repeater {
        model: root.donutCharts ? quotaPresentation.quotaProviders : []
        delegate: ProviderMeter {
            required property var modelData
            required property int index
            objectName: "provider-meter-" + modelData.id
            provider: modelData
            width: root.ringSize
            height: width
            x: root.vertical ? (root.width - width) / 2 : index * (width + root.ringSpacing)
            y: root.vertical ? index * (height + root.ringSpacing) : (root.height - height) / 2
            sessionColor: root.sessionColor
            weeklyColor: root.weeklyColor
            toolTipsEnabled: root.toolTipsEnabled
            toolTipLocation: root.toolTipLocation
        }
    }
}
