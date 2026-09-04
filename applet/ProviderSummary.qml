pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import plasma.applet.org.kyaulabs.codexbar as Private

QQC2.ItemDelegate {
    id: root

    required property var provider
    property int windowLimit: 2
    property string badgeText
    readonly property color accentColor: provider.display && provider.display.accentColor
                                         ? provider.display.accentColor :
                                           Kirigami.Theme.highlightColor
    readonly property string errorText: typeof provider.error === "string" ? provider.error : (
                                                                                 provider.error
                                                                                 && provider.error.message
                                                                                 || "")
    readonly property var windows: {
        const allWindows = provider.windows ? provider.windows.filter(windowData =>
        !windowData.idle) : []
        return windowLimit > 0 ? allWindows.slice(0, windowLimit) : allWindows
    }

    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing

    contentItem: RowLayout {
        id: content

        spacing: Kirigami.Units.largeSpacing

        Rectangle {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Layout.preferredWidth
            radius: width / 2
            color: root.accentColor

            QQC2.Label {
                anchors.centerIn: parent
                text: (root.provider.name || root.provider.label || root.provider.id).charAt(
                          0).toUpperCase()
                color: "white"
                font.bold: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true

                QQC2.Label {
                    Layout.fillWidth: true
                    text: root.provider.name || root.provider.label || root.provider.id
                    font.bold: true
                    elide: Text.ElideRight
                }
                QQC2.Label {
                    visible: root.badgeText.length > 0
                    text: root.badgeText
                    color: Kirigami.Theme.highlightColor
                    font: Kirigami.Theme.smallFont
                }
            }

            Repeater {
                model: root.windows

                delegate: ColumnLayout {
                    id: usageSummary

                    required property var modelData

                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: Private.PresentationFormatter.windowTitle(String(
                                                                                usageSummary.modelData.kind
                                                                                || ""), String(
                                                                                usageSummary.modelData.label
                                                                                || ""))
                            font: Kirigami.Theme.smallFont
                        }
                        QQC2.Label {
                            text: Private.PresentationFormatter.remainingLabel(
                                      usageSummary.modelData.remainingPercent)
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }

                    UsageBar {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.smallSpacing
                        percent: Number(usageSummary.modelData.usedPercent ?? (100 - Number(
                                                                                   usageSummary.modelData.remainingPercent
                                                                                   ?? 100)))
                        accentColor: root.accentColor
                    }
                }
            }

            QQC2.Label {
                visible: root.errorText.length > 0
                text: root.errorText
                color: Kirigami.Theme.negativeTextColor
                font: Kirigami.Theme.smallFont
            }

            QQC2.Label {
                visible: root.windows.length === 0 && root.errorText.length === 0
                text: root.provider.status && root.provider.status.label
                      ? root.provider.status.label : qsTr("No quota windows")
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
            }
        }
    }
}
