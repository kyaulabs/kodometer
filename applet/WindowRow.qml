import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import plasma.applet.org.kyaulabs.codexbar as Private

ColumnLayout {
    id: root

    required property var windowData
    property color accentColor: Kirigami.Theme.highlightColor
    property int clockTick: 0
    readonly property real usedPercent: Private.PresentationFormatter.clampPercent(Number(
                                                                                       windowData.usedPercent
                                                                                       ?? (100 - Number(
                                                                                               windowData.remainingPercent
                                                                                               ?? 100))))
    readonly property string title: Private.PresentationFormatter.windowTitle(String(
                                                                                  windowData.kind
                                                                                  || ""), String(
                                                                                  windowData.label
                                                                                  || ""))
    readonly property string resetText: {
        root.clockTick
        return Private.PresentationFormatter.resetLabel(String(windowData.resetAt || ""))
    }

    spacing: Kirigami.Units.smallSpacing

    QQC2.Label {
        Layout.fillWidth: true
        text: root.title
        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.08
        font.bold: true
    }

    UsageBar {
        Layout.fillWidth: true
        Layout.preferredHeight: Kirigami.Units.smallSpacing
        percent: root.usedPercent
        accentColor: root.accentColor
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.Label {
            text: qsTr("%1% used").arg(Math.round(root.usedPercent * 10) / 10)
            font: Kirigami.Theme.smallFont
        }

        Item {
            Layout.fillWidth: true
        }

        QQC2.Label {
            visible: text.length > 0
            text: root.resetText.length > 0 ? qsTr("Resets %1").arg(root.resetText) : ""
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }
    }
}
