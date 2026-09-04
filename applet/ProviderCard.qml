pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: root

    required property var provider
    readonly property color accentColor: provider.display && provider.display.accentColor
                                         ? provider.display.accentColor :
                                           Kirigami.Theme.highlightColor

    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    radius: Kirigami.Units.cornerRadius
    color: Kirigami.Theme.alternateBackgroundColor

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true

            Rectangle {
                implicitWidth: Kirigami.Units.gridUnit
                implicitHeight: width
                radius: width / 2
                color: root.accentColor

                Text {
                    anchors.centerIn: parent
                    text: root.provider.name ? root.provider.name.charAt(0).toUpperCase() : "?"
                    color: "white"
                    font.bold: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Kirigami.Heading {
                    level: 4
                    text: root.provider.name || root.provider.id
                }

                Kirigami.SelectableLabel {
                    visible: text.length > 0
                    text: root.provider.identity ? [root.provider.identity.plan,
                                                    root.provider.identity.accountEmail].filter(
                                                       value => value).join(" · ") : ""
                    opacity: 0.7
                    font: Kirigami.Theme.smallFont
                }
            }
        }

        Repeater {
            model: root.provider.windows || []

            ColumnLayout {
                id: windowRow

                required property var modelData
                Layout.fillWidth: true
                visible: !modelData.idle
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: windowRow.modelData.label || windowRow.modelData.kind
                    }
                    QQC2.Label {
                        text: Math.round(windowRow.modelData.remainingPercent) + "% left"
                        font: Kirigami.Theme.smallFont
                    }
                }

                UsageBar {
                    Layout.fillWidth: true
                    percent: 100 - Number(windowRow.modelData.remainingPercent)
                    accentColor: root.accentColor
                }
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.provider.error !== null && root.provider.error !== undefined
            type: Kirigami.MessageType.Error
            text: root.provider.error ? (root.provider.error.message || String(root.provider.error)) :
                                        ""
        }
    }
}
