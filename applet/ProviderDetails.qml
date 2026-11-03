pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import plasma.applet.org.kyaulabs.kodometer as Private

PaginatedPage {
    id: root

    required property var provider
    readonly property string providerId: String(provider.id || "")
    onProviderIdChanged: currentPage = 0
    property int clockTick: 0
    property bool showIdleWindows: false
    property bool fillRemaining: true
    property bool openaiTabbed: true
    property int openaiSection: 0
    readonly property bool isOpenAI: providerId === "openai"
    readonly property bool planVisible: !isOpenAI || !openaiTabbed || openaiSection === 0
    readonly property bool apiVisible: isOpenAI && (!openaiTabbed || openaiSection === 1)
    readonly property var apiData: provider.api || ({})
    readonly property var apiCost: apiData.cost || ({})
    property var quotaHistory: null
    readonly property var historySeries: {
        if (!quotaHistory)
            return []
        quotaHistory.revision
        return quotaHistory.view(String(provider.id || ""))
    }
    property var switching: null
    property bool loading: false
    readonly property color accentColor: provider.display && provider.display.accentColor
                                         ? provider.display.accentColor :
                                           Kirigami.Theme.highlightColor
    readonly property var windows: planVisible && provider.windows ? provider.windows.filter(
                                                                         windowData
                                                                         => root.showIdleWindows ||
                                                                            !windowData.idle) : []
    readonly property var identity: planVisible ? provider.identity || ({}) : ({})
    readonly property var statusData: planVisible ? provider.status || ({}) : ({})
    readonly property var credits: planVisible ? provider.credits || ({}) : ({})
    readonly property var cost: planVisible ? provider.cost || ({}) : ({})
    readonly property var detailSections: planVisible ? provider.details || [] : []
    readonly property var accounts: planVisible ? provider.accounts || [] : []
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

    contentItem: ColumnLayout {
        id: details
        objectName: "pageContent"

        parent: root.contentHost
        width: root.width
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true

            ProviderIcon {
                Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                Layout.preferredHeight: Layout.preferredWidth
                providerId: String(root.provider.id || "")
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                QQC2.Label {
                    Layout.fillWidth: true
                    text: root.provider.name || root.provider.id
                    textFormat: Text.PlainText
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

        QQC2.TabBar {
            objectName: "openaiServiceTabs"
            Layout.fillWidth: true
            visible: root.isOpenAI && root.openaiTabbed
            currentIndex: root.openaiSection
            onCurrentIndexChanged: root.openaiSection = currentIndex
            QQC2.TabButton {
                text: qsTr("Plan")
            }
            QQC2.TabButton {
                text: qsTr("API")
            }
        }

        QQC2.Label {
            objectName: "openaiPlanUnavailable"
            Layout.fillWidth: true
            visible: root.isOpenAI && root.planVisible && Boolean(root.provider.apiOnly)
            text: qsTr("No plan usage available. Configure a Codex login in OpenAI OAuth profiles.")
            wrapMode: Text.WordWrap
        }

        QQC2.Label {
            objectName: "pendingAccountUsage"
            Layout.fillWidth: true
            visible: Boolean(root.provider.pendingSelection)
            text: root.loading ? qsTr("Loading usage for this selection…") : qsTr(
                                     "No usage for this selection. Refresh or choose another account.")
            wrapMode: Text.WordWrap
        }

        QQC2.Label {
            objectName: "selectedOAuthProfile"
            Layout.fillWidth: true
            visible: root.planVisible && Boolean(root.provider.profileName)
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

        QQC2.Label {
            objectName: "bankedResets"
            Layout.fillWidth: true
            visible: root.planVisible && Number(root.provider.bankedResets || 0) >= 1
            text: qsTr("Banked resets: %1").arg(root.provider.bankedResets || 0)
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: root.windows

            delegate: WindowRow {
                required property var modelData

                Layout.fillWidth: true
                windowData: modelData
                fillRemaining: root.fillRemaining
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
                    fillRemaining: root.fillRemaining
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

        QuotaHistoryChart {
            objectName: "quotaHistoryChart"
            Layout.fillWidth: true
            visible: ["openai", "claude", "gemini", "kimi", "zai"].includes(String(
                                                                                root.provider.id))
            && windows.length > 0
            windows: root.windows.filter(windowData => windowData.kind !== "spend-limit")
            series: root.historySeries
            accentColor: root.accentColor
            endTime: {
                root.clockTick
                root.historySeries
                return Date.now() / 1000
            }
        }

        ColumnLayout {
            objectName: "openaiApiSection"
            Layout.fillWidth: true
            visible: root.apiVisible
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: qsTr("OpenAI API Platform")
                font.bold: true
            }
            QQC2.Label {
                Layout.fillWidth: true
                visible: !root.apiData.configured
                text: qsTr(
                          "Store an organization Admin API key in Provider credentials to view spending. Plan credentials and ordinary API keys cannot provide organization costs.")
                wrapMode: Text.WordWrap
            }
            QQC2.Label {
                objectName: "openaiMonthToDate"
                visible: root.apiCost.monthToDateUSD !== undefined
                text: qsTr("Month to date (reported): %1").arg(
                          Private.PresentationFormatter.usdLabel(Number(root.apiCost.monthToDateUSD
                                                                        || 0)))
            }
            CostHistory {
                objectName: "openaiApiHistory"
                Layout.fillWidth: true
                cost: root.apiCost
                showNotes: false
                accentColor: root.accentColor
            }
        }

        CostHistory {
            objectName: "costHistory"
            Layout.fillWidth: true
            cost: root.cost
            accentColor: root.accentColor
        }

        ProviderActionRow {
            objectName: "providerActionRow"
            Layout.fillWidth: true
            providerId: String(root.provider.id || "")
            region: String(root.provider.region || "")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.errorText.length > 0 || Boolean(root.provider.accountsError)
            type: Kirigami.MessageType.Error
            text: root.errorText || root.provider.accountsError || ""
        }

        QQC2.Label {
            Layout.fillWidth: true
            visible: root.planVisible && Boolean(root.provider.source)
            text: root.provider.source || ""
            horizontalAlignment: Text.AlignRight
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
        }
    }
}
