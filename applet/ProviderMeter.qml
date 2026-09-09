pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore

Item {
    id: root
    required property var provider
    property string sessionColor
    property string weeklyColor
    property bool toolTipsEnabled: true
    property int toolTipLocation: PlasmaCore.Types.Floating
    readonly property var windows: provider.windows || []
    readonly property color accentColor: provider.display && provider.display.accentColor
                                         ? provider.display.accentColor :
                                           Kirigami.Theme.highlightColor
    readonly property real strokeWidth: Math.max(0, Math.min(width, height) / (windows.length * 3
                                                                               + 5))

    readonly property string quotaDescription: windows.map((windowData, index) => qsTr(
                                                                                      "%1: %2% remaining (%3)").arg(
                                                                                      String(windowData.label
                                                                                             || windowData.kind)).arg(
                                                                                      Number(windowData.remainingPercent).toFixed(
                                                                                          1)).arg(index
                                                                                                  === 0 ? qsTr(
                                                                                                              "outer ring") :
                                                                                                          qsTr("ring %1").arg(
                                                                                                              index + 1))).join(
                                                   "\n")

    function windowColor(index) {
        const kind = windows[index].kind
        const custom = kind === "session" ? sessionColor : kind === "weekly" ? weeklyColor : ""
        if (!(provider.display && provider.display.customAccent) && /^#[0-9a-fA-F]{6}$/.test(
                    custom))
            return custom
        let hueOffset = kind === "session" ? 0 : kind === "weekly" ? 0.17 : 0.34
        if (kind !== "session" && kind !== "weekly") {
            for (let i = 0; i < kind.length; ++i)
                hueOffset = (hueOffset + kind.charCodeAt(i) * 0.013) % 1
        }
        return hueOffset === 0 ? accentColor : Qt.hsla((Math.max(0, accentColor.hslHue)
                                                        + hueOffset) % 1, Math.max(0.55,
                                                                                   accentColor.hslSaturation),
                                                       accentColor.hslLightness, 1)
    }

    implicitWidth: 48
    implicitHeight: 48
    Accessible.role: Accessible.ProgressBar
    Accessible.name: String(provider.name || provider.id)
    Accessible.description: quotaDescription

    Repeater {
        model: root.windows
        delegate: QuotaRing {
            required property var modelData
            required property int index
            objectName: "quota-ring-" + root.provider.id + "-" + modelData.kind
            anchors.centerIn: parent
            width: Math.max(0, Math.min(root.width, root.height) - index * root.strokeWidth * 3)
            height: width
            strokeWidth: root.strokeWidth
            showValue: false
            remaining: modelData.remainingPercent
            accentColor: root.windowColor(index)
            quotaLabel: String(modelData.label || modelData.kind)
        }
    }

    ProviderIcon {
        anchors.centerIn: parent
        providerId: String(root.provider.id)
        width: Math.max(0, Math.min(root.width, root.height) - root.windows.length
                        * root.strokeWidth * 3) * 0.75
        height: width
        visible: width >= 10
    }

    PlasmaCore.ToolTipArea {
        objectName: "provider-tooltip-" + root.provider.id
        anchors.fill: parent
        active: root.toolTipsEnabled
        mainText: qsTr("Kodometer — %1").arg(String(root.provider.name || root.provider.id))
        subText: root.quotaDescription
        textFormat: Text.PlainText
        location: root.toolTipLocation
    }
}
