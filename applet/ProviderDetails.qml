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
    property bool showIdleWindows: false
    readonly property color accentColor: provider.display && provider.display.accentColor
                                         ? provider.display.accentColor :
                                           Kirigami.Theme.highlightColor
    readonly property var windows: provider.windows ? provider.windows.filter(windowData
                                                                              => root.showIdleWindows
                                                                                 || !windowData.idle) :
                                                      []
    readonly property var identity: provider.identity || ({})
    readonly property var statusData: provider.status || ({})
    readonly property var credits: provider.credits || ({})
    readonly property var cost: provider.cost || ({})
    readonly property var detailSections: provider.details || []
    readonly property var accounts: provider.accounts || []
    readonly property string errorText: typeof provider.error === "string" ? provider.error : (
                                                                                 provider.error
                                                                                 && provider.error.message
                                                                                 || "")
    readonly property bool hasCredits: credits.remaining !== undefined && credits.remaining !== null
    readonly property bool hasBalance: cost.balance !== undefined || cost.balanceUSD !== undefined
    readonly property real balance: Number(cost.balance !== undefined ? cost.balance :
                                                                        cost.balanceUSD)
    readonly property string balanceCurrency: String(cost.currencyCode || "USD")
    readonly property bool hasBalanceBreakdown: cost.toppedUpBalance !== undefined
                                                || cost.grantedBalance !== undefined
    readonly property bool hasCost: root.hasBalance || cost.todayUSD !== undefined
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
                    visible: Boolean(root.provider.updatedAt)
                    text: qsTr("Updated %1").arg(root.provider.updatedAt || "")
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                }
            }

            Rectangle {
                visible: Boolean(root.statusData.label)
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

        QQC2.Label {
            objectName: "selectedOAuthProfile"
            Layout.fillWidth: true
            visible: Boolean(root.provider.profileName)
            text: qsTr("Profile: %1").arg(root.provider.profileName || "")
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
        }

        QQC2.Label {
            objectName: "selectedWalletAccount"
            Layout.fillWidth: true
            visible: Boolean(root.provider.accountName)
            text: qsTr("Account: %1").arg(root.provider.accountName || "")
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
        }

        ProviderActionRow {
            objectName: "providerActionRow"
            Layout.fillWidth: true
            providerId: String(root.provider.id || "")
            region: String(root.provider.region || "")
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            visible: Boolean(root.identity.accountEmail || root.identity.plan)
        }

        RowLayout {
            Layout.fillWidth: true
            visible: Boolean(root.identity.accountEmail || root.identity.plan)

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

        Repeater {
            model: root.detailSections

            delegate: DetailSection {
                required property var modelData

                Layout.fillWidth: true
                section: modelData
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
                    showIdleWindows: root.showIdleWindows
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
                text: qsTr("Billing")
                font.bold: true
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.08
            }
            QQC2.Label {
                visible: root.hasBalance
                text: qsTr("%1: %2").arg(root.cost.period || qsTr("Balance")).arg(
                          Private.PresentationFormatter.creditsLabel(root.balance,
                                                                     root.balanceCurrency))
            }
            QQC2.Label {
                visible: root.hasBalanceBreakdown
                text: qsTr("Paid: %1 · Granted: %2").arg(Private.PresentationFormatter.creditsLabel(
                                                             Number(root.cost.toppedUpBalance || 0),
                                                             root.balanceCurrency)).arg(
                          Private.PresentationFormatter.creditsLabel(Number(
                                                                         root.cost.grantedBalance
                                                                         || 0), root.balanceCurrency))
                color: Kirigami.Theme.disabledTextColor
            }
            QQC2.Label {
                visible: root.cost.spent !== undefined
                text: qsTr("Spent: %1").arg(Private.PresentationFormatter.creditsLabel(Number(
                                                                                           root.cost.spent
                                                                                           || 0), root.balanceCurrency))
                color: Kirigami.Theme.disabledTextColor
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

        CostHistory {
            objectName: "costHistory"
            Layout.fillWidth: true
            cost: root.cost
            accentColor: root.accentColor
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.errorText.length > 0 || Boolean(root.provider.accountsError)
            type: Kirigami.MessageType.Error
            text: root.errorText || root.provider.accountsError || ""
        }

        QQC2.Label {
            Layout.fillWidth: true
            visible: Boolean(root.provider.source)
            text: root.provider.source || ""
            horizontalAlignment: Text.AlignRight
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
        }
    }

    QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
}
