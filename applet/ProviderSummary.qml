pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import plasma.applet.org.kyaulabs.kodometer as Private

QQC2.ItemDelegate {
    id: root

    required property var provider
    property int windowLimit: 0
    property bool fillRemaining: true
    property bool showIdleWindows: false
    property string badgeText
    readonly property color accentColor: provider.display && provider.display.accentColor
                                         ? provider.display.accentColor :
                                           Kirigami.Theme.highlightColor
    readonly property string errorText: typeof provider.error === "string" ? provider.error : (
                                                                                 provider.error
                                                                                 && provider.error.message
                                                                                 || "")
    readonly property var cost: provider.cost || ({})
    readonly property bool hasBalance: cost.balance !== undefined || cost.balanceUSD !== undefined
    readonly property real balance: Number(cost.balance !== undefined ? cost.balance :
                                                                        cost.balanceUSD)
    readonly property string balanceCurrency: String(cost.currencyCode || "USD")
    readonly property var windows: {
        const allWindows = provider.windows ? provider.windows.filter(windowData
                                                                      => root.showIdleWindows ||
                                                                         !windowData.idle) : []
        return windowLimit > 0 ? allWindows.slice(0, windowLimit) : allWindows
    }

    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing

    contentItem: RowLayout {
        id: content

        spacing: Kirigami.Units.largeSpacing

        ProviderIcon {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Layout.preferredWidth
            providerId: root.provider.id
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true

                QQC2.Label {
                    Layout.fillWidth: true
                    text: root.provider.name || root.provider.label || root.provider.id
                    textFormat: Text.PlainText
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

            QQC2.Label {
                objectName: "bankedResets"
                Layout.fillWidth: true
                visible: Number(root.provider.bankedResets || 0) >= 1
                text: qsTr("Banked resets: %1").arg(root.provider.bankedResets || 0)
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                font: Kirigami.Theme.smallFont
            }

            Repeater {
                model: root.windows

                delegate: ColumnLayout {
                    id: usageSummary

                    required property var modelData
                    readonly property real usedPercent: Number(modelData.usedPercent ?? (100
                                                                                         - Number(modelData.remainingPercent
                                                                                                  ?? NaN)))

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
                            text: root.fillRemaining ? Private.PresentationFormatter.remainingLabel(
                                                           usageSummary.modelData.remainingPercent) :
                                                       (Number.isFinite(usageSummary.usedPercent)
                                                        ? qsTr("%1% used").arg(Math.max(0, Math.min(
                                                                                            100, usageSummary.usedPercent)).toFixed(
                                                                                   1)) : qsTr(
                                                              "Not reported"))
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }

                    UsageBar {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.smallSpacing * 2.5
                        objectName: "summaryQuotaBar"
                        percent: root.fillRemaining ? Number(
                                                          usageSummary.modelData.remainingPercent
                                                          ?? NaN) : usageSummary.usedPercent
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
                      ? root.provider.status.label : (root.hasBalance ? qsTr("Balance: %1").arg(
                                                                            Private.PresentationFormatter.creditsLabel(
                                                                                root.balance,
                                                                                root.balanceCurrency)) :
                                                                        qsTr("No quota windows"))
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
            }
        }
    }
}
