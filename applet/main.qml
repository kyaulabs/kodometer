pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import plasma.applet.org.kyaulabs.kodometer as Private

PlasmoidItem {
    id: root

    switchWidth: Kirigami.Units.gridUnit * 10
    switchHeight: Kirigami.Units.gridUnit * 10
    Plasmoid.icon: "kodometer"
    Plasmoid.status: backend.busy ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus
    toolTipMainText: panelToolTip.mainText
    toolTipSubText: panelToolTip.subText
    toolTipTextFormat: Text.PlainText
    property var settings: Plasmoid.configuration
    property int clockTick: 0

    PanelToolTip {
        id: panelToolTip
        providers: presentation.displayedProviders
        donutCharts: false
    }

    QuotaPresentation {
        id: presentation
        providers: backend.providers
        showSpark: Plasmoid.configuration.showCodexSpark
        showIdleWindows: Plasmoid.configuration.showIdleWindows
        hiddenWindows: Plasmoid.configuration.hiddenQuotaWindows
        providerColors: Plasmoid.configuration.providerColors
        onCatalogChanged: {
            const encoded = JSON.stringify(catalog)
            if (catalog.length > 0 && Plasmoid.configuration.quotaWindowCatalog !== encoded) {
                Plasmoid.configuration.quotaWindowCatalog = encoded
                root.settings.writeConfig()
            }
        }
    }

    function remainingFor(kind) {
        let remaining = null
        for (const provider of presentation.displayedProviders) {
            for (const windowData of (provider.windows || [])) {
                const value = windowData.remainingPercent
                if (windowData.kind === kind && typeof value === "number" && Number.isFinite(value))
                    remaining = remaining === null ? value : Math.min(remaining, value)
            }
        }
        return remaining === null ? NaN : remaining
    }

    Private.KWalletCredentialStore {
        id: credentialStore
    }

    Private.UsageController {
        id: backend
        credentialStore: credentialStore
        history.persistent: true
        deepseekAccountId: Plasmoid.configuration.deepseekAccountId
        kimiAccountId: Plasmoid.configuration.kimiAccountId
        openrouterAccountId: Plasmoid.configuration.openrouterAccountId
        xaiAccountId: Plasmoid.configuration.xaiAccountId
        zaiAccountId: Plasmoid.configuration.zaiAccountId
        profiles.configuration: Plasmoid.configuration.oauthProfiles
        autoRefresh: Plasmoid.configuration.autoRefresh
        refreshIntervalMinutes: Plasmoid.configuration.refreshIntervalMinutes
        disabledProviders: Plasmoid.configuration.disabledProviders
        onProviderContextChanged: provider => {
            if (quotaNotifier)
                quotaNotifier.forgetProvider(provider)
        }
        onProviderRefreshed: provider => quotaNotifier.observe(provider)
    }

    Private.QuotaNotifier {
        id: quotaNotifier
        enabled: Plasmoid.configuration.quotaNotifications
        thresholdPercent: Plasmoid.configuration.quotaNotificationThreshold
    }

    AccountSwitching {
        id: accountSwitching
        configuration: Plasmoid.configuration
        accounts: credentialStore.accounts
        onSelectionApplied: providerId => usageNavigation.focusProvider(providerId)
    }

    UsageNavigation {
        id: usageNavigation
        providers: presentation.displayedProviders
        catalog: accountSwitching.providers
    }

    Timer {
        interval: 60000
        repeat: true
        running: root.expanded
        onTriggered: root.clockTick++
    }

    compactRepresentation: Item {
        id: compactView
        readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
        readonly property bool donuts: Plasmoid.configuration.panelDonutCharts
        readonly property int meterCount: Math.max(1, compactMeter.providerCount)
        Layout.minimumWidth: donuts && !vertical ? height * meterCount + 4 * (meterCount - 1) : 0
        Layout.minimumHeight: donuts && vertical ? width * meterCount + 4 * (meterCount - 1) : 0
        Layout.preferredWidth: donuts && !vertical ? Layout.minimumWidth : -1
        Layout.preferredHeight: donuts && vertical ? Layout.minimumHeight : -1
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Kodometer usage")
        Accessible.description: compactMeter.quotaDescription
        Accessible.onPressAction: root.expanded = !root.expanded
        activeFocusOnTab: true
        Keys.onSpacePressed: root.expanded = !root.expanded
        Keys.onReturnPressed: root.expanded = !root.expanded

        CompactMeter {
            id: compactMeter
            anchors.fill: parent
            donutCharts: compactView.donuts
            vertical: compactView.vertical
            systemAccent: Plasmoid.configuration.panelSystemAccent
            providers: presentation.displayedProviders
            toolTipsEnabled: !root.expanded
            toolTipLocation: Plasmoid.location
            sessionRemaining: root.remainingFor("session")
            weeklyRemaining: root.remainingFor("weekly")
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.expanded = !root.expanded
        }
    }

    fullRepresentation: UsagePopup {
        availableHeight: root.availableScreenRect.height > 0 ? root.availableScreenRect.height :
                                                               Screen.height
        navigation: usageNavigation
        switching: accountSwitching
        quotaHistory: backend.history
        busy: backend.busy
        generatedAt: String(backend.snapshot.generatedAt || "")
        error: backend.error
        profileError: !backend.profiles.valid && accountSwitching.providers.some(provider
                                                                                 => provider.kind
                                                                                    === "profile")
                      ? backend.profiles.error : ""
        clockTick: root.clockTick
        showIdleWindows: Plasmoid.configuration.showIdleWindows
        fillRemaining: Plasmoid.configuration.quotaBarsRemaining
        onRefreshRequested: {
            credentialStore.open()
            backend.refresh()
        }
        onWalletRequested: credentialStore.open()
        onConfigureRequested: Plasmoid.internalAction("configure").trigger()
    }

    Component.onCompleted: {
        credentialStore.open()
        backend.refresh()
    }
}
