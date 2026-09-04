pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import plasma.applet.org.kyaulabs.kodometer as Private

Flickable {
    id: root

    required property var provider
    property int clockTick: 0
    readonly property color accentColor: provider.display && provider.display.accentColor
                                         ? provider.display.accentColor :
                                           Kirigami.Theme.highlightColor
    readonly property var windows: provider.windows ? provider.windows.filter(windowData =>
    !windowData.idle) : []
    readonly property var identity: provider.identity || ({})
    readonly property var statusData: provider.status || ({})
    readonly property var credits: provider.credits || ({})
    readonly property var cost: provider.cost || ({})
    readonly property var accounts: provider.accounts || []
    readonly property string errorText: typeof provider.error === "string" ? provider.error : (
                                                                                 provider.error
                                                                                 && provider.error.message
                                                                                 || "")
    readonly property bool hasCredits: credits.remaining !== undefined && credits.remaining !== null
    readonly property bool hasCost: cost.balanceUSD !== undefined || cost.todayUSD !== undefined
                                    || cost.last30DaysUSD !== undefined || cost.usedUSD
                                    !== undefined

    contentWidth: width
    contentHeight: details.implicitHeight + Kirigami.Units.largeSpacing
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: details

        width: root.width
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                QQC2.Label {
                    Layout.fillWidth: true
                    text: root.provider.name || root.provider.id
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.35
                    font.bold: true
                    elide: Text.ElideRight
                }

                QQC2.Label {
                    visible: root.provider.updatedAt
                    text: qsTr("Updated %1").arg(root.provider.updatedAt || "")
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                }
            }

            Rectangle {
                visible: root.statusData.label
                radius: height / 2
                color: root.statusData.level === "critical"
                       ? Kirigami.Theme.negativeBackgroundColor :
                         Kirigami.Theme.neutralBackgroundColor
                implicitWidth: statusLabel.implicitWidth + Kirigami.Units.largeSpacing
                implicitHeight: statusLabel.implicitHeight + Kirigami.Units.smallSpacing

                QQC2.Label {
                    id: statusLabel
                    anchors.centerIn: parent
                    text: root.statusData.label || ""
                    font: Kirigami.Theme.smallFont
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            visible: root.identity.accountEmail || root.identity.plan
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.identity.accountEmail || root.identity.plan

            QQC2.Label {
                Layout.fillWidth: true
                text: root.identity.accountEmail || qsTr("Account")
                elide: Text.ElideMiddle
            }
            QQC2.Label {
                text: root.identity.plan || ""
                color: Kirigami.Theme.disabledTextColor
            }
        }

        Repeater {
            model: root.windows

            delegate: WindowRow {
                required property var modelData

                Layout.fillWidth: true
                windowData: modelData
                accentColor: root.accentColor
                clockTick: root.clockTick
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.accounts.length > 0
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Separator {
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: qsTr("Accounts")
                font.bold: true
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.08
            }
            Repeater {
                model: root.accounts

                delegate: ProviderSummary {
                    required property var modelData

                    Layout.fillWidth: true
                    provider: modelData
                    windowLimit: 0
                    badgeText: modelData.active ? qsTr("Active") : ""
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            visible: root.hasCredits || root.hasCost
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.hasCredits
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: qsTr("Credits")
                font.bold: true
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.08
            }
            QQC2.Label {
                text: Private.PresentationFormatter.creditsLabel(Number(root.credits.remaining),
                                                                 String(root.credits.unit
                                                                        || "credits"))
                color: Kirigami.Theme.disabledTextColor
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.hasCost
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: qsTr("Cost")
                font.bold: true
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.08
            }
            QQC2.Label {
                visible: root.cost.balanceUSD !== undefined
                text: qsTr("Prepaid balance: %1").arg(Private.PresentationFormatter.usdLabel(Number(
                                                                                                 root.cost.balanceUSD
                                                                                                 || 0)))
            }
            QQC2.Label {
                visible: root.cost.usedUSD !== undefined && root.cost.limitUSD !== undefined
                text: qsTr("%1: %2 / %3").arg(root.cost.period || qsTr("Monthly cap")).arg(
                          Private.PresentationFormatter.usdLabel(Number(root.cost.usedUSD
                                                                        || 0))).arg(
                          Private.PresentationFormatter.usdLabel(Number(root.cost.limitUSD || 0)))
            }
            QQC2.Label {
                visible: root.cost.todayUSD !== undefined
                text: qsTr("Today: %1").arg(Private.PresentationFormatter.usdLabel(Number(
                                                                                       root.cost.todayUSD
                                                                                       || 0)))
            }
            QQC2.Label {
                visible: root.cost.last30DaysUSD !== undefined
                text: qsTr("%1: %2").arg(root.cost.historyPartial ? qsTr("Last 30 days (partial)") :
                                                                    qsTr("Last 30 days")).arg(
                          Private.PresentationFormatter.usdLabel(Number(root.cost.last30DaysUSD
                                                                        || 0)))
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.errorText.length > 0 || root.provider.accountsError
            type: Kirigami.MessageType.Error
            text: root.errorText || root.provider.accountsError || ""
        }

        QQC2.Label {
            Layout.fillWidth: true
            visible: root.provider.source
            text: root.provider.source || ""
            horizontalAlignment: Text.AlignRight
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
        }
    }

    QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
}
