pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
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

    PanelToolTip {
        id: panelToolTip
        providers: backend.providers
        donutCharts: Plasmoid.configuration.panelDonutCharts
    }

    property int clockTick: 0

    function remainingFor(kind) {
        let remaining = null
        for (const provider of backend.providers) {
            const windows = provider.windows || []
            for (const windowData of windows) {
                if (windowData.kind !== kind || windowData.remainingPercent === null
                        || windowData.remainingPercent === undefined) {
                    continue
                }
                const value = Number(windowData.remainingPercent)
                if (Number.isFinite(value))
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
        deepseekAccountId: Plasmoid.configuration.deepseekAccountId
        kimiAccountId: Plasmoid.configuration.kimiAccountId
        openrouterAccountId: Plasmoid.configuration.openrouterAccountId
        xaiAccountId: Plasmoid.configuration.xaiAccountId
        zaiAccountId: Plasmoid.configuration.zaiAccountId
        profiles.configuration: Plasmoid.configuration.oauthProfiles
        onProviderContextChanged: provider => {
            if (quotaNotifier)
                quotaNotifier.forgetProvider(provider)
        }
        autoRefresh: Plasmoid.configuration.autoRefresh
        refreshIntervalMinutes: Plasmoid.configuration.refreshIntervalMinutes
        disabledProviders: Plasmoid.configuration.disabledProviders
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
        onSelectionApplied: providerId => navigation.focusProvider(providerId)
    }

    UsageNavigation {
        id: navigation
        providers: backend.providers
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
        Layout.minimumWidth: donuts && !vertical ? height * 2 + 4 : 0
        Layout.minimumHeight: donuts && vertical ? width * 2 + 4 : 0
        Layout.preferredWidth: donuts && !vertical ? height * 2 + 4 : -1
        Layout.preferredHeight: donuts && vertical ? width * 2 + 4 : -1
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
            sessionColor: Plasmoid.configuration.panelSessionColor
            weeklyColor: Plasmoid.configuration.panelWeeklyColor
            providers: backend.providers
            toolTipsEnabled: !root.expanded
            toolTipLocation: Plasmoid.location
            sessionRemaining: root.remainingFor("session")
            weeklyRemaining: root.remainingFor("weekly")
        }

        MouseArea {
            id: compactMouse
            anchors.fill: parent
            onClicked: root.expanded = !root.expanded
        }
    }

    fullRepresentation: Item {
        id: fullView
        implicitWidth: Kirigami.Units.gridUnit * 22
        implicitHeight: Kirigami.Units.gridUnit * 34
        Layout.minimumWidth: Kirigami.Units.gridUnit * 18
        Layout.minimumHeight: Kirigami.Units.gridUnit * 22
        focus: true

        Keys.onLeftPressed: navigation.selectPrevious()
        Keys.onRightPressed: navigation.selectNext()

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing

            BrandPalette {
                id: brand
            }

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
                source: brand.wordmark
                fillMode: Image.PreserveAspectFit
                sourceSize.width: Math.ceil(width * Screen.devicePixelRatio)
                sourceSize.height: Math.ceil(height * Screen.devicePixelRatio)
                Accessible.role: Accessible.Graphic
                Accessible.name: qsTr("Kodometer")
            }

            ProviderTabs {
                Layout.fillWidth: true
                visible: providers.length > 0
                providers: navigation.displayedProviders
                overviewVisible: providers.length > 1
                selectedIndex: navigation.selectedTabIndex
                onOverviewSelected: navigation.selectOverview()
                onProviderSelected: providerId => navigation.selectProvider(providerId)
            }

            Kirigami.Separator {
                Layout.fillWidth: true
                visible: navigation.displayedProviders.length > 0
            }

            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                active: navigation.displayedProviders.length > 0
                sourceComponent: navigation.overviewSelected ? overviewComponent : providerComponent
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: navigation.displayedProviders.length === 0

                Item {
                    Layout.fillHeight: true
                }
                Kirigami.Icon {
                    Layout.alignment: Qt.AlignHCenter
                    source: backend.busy ? "view-refresh" : Qt.resolvedUrl(
                                               "assets/kodometer-symbolic.svg")
                    isMask: !backend.busy
                    implicitWidth: Kirigami.Units.iconSizes.large
                    implicitHeight: width
                }
                QQC2.Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: backend.busy ? qsTr("Loading provider usage…") : qsTr("No provider data")
                }
                QQC2.Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Configure providers…")
                    onClicked: Plasmoid.internalAction("configure").trigger()
                }
                Item {
                    Layout.fillHeight: true
                }
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: !backend.profiles.valid && accountSwitching.providers.some(provider
                                                                                    => provider.kind
                                                                                       === "profile")
                type: Kirigami.MessageType.Error
                text: backend.profiles.error
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: backend.error.length > 0
                type: Kirigami.MessageType.Error
                text: backend.error
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: accountSwitching.error.length > 0
                type: Kirigami.MessageType.Error
                text: accountSwitching.error
            }

            RowLayout {
                Layout.fillWidth: true

                QQC2.ToolButton {
                    objectName: "openAccountSwitcher"
                    icon.name: "user-identity"
                    text: qsTr("Switch account…")
                    display: QQC2.AbstractButton.IconOnly
                    enabled: accountSwitching.providers.length > 0
                    onClicked: accountDialog.open()
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                }

                QQC2.BusyIndicator {
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: width
                    running: backend.busy
                    visible: running
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    text: backend.snapshot.generatedAt ? qsTr("Snapshot %1").arg(
                                                             backend.snapshot.generatedAt) : ""
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                    elide: Text.ElideRight
                }

                QQC2.ToolButton {
                    icon.name: "view-refresh"
                    text: qsTr("Refresh")
                    display: QQC2.AbstractButton.IconOnly
                    enabled: !backend.busy
                    onClicked: {
                        credentialStore.open()
                        backend.refresh()
                    }
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                }
            }
        }

        AccountSwitchDialog {
            id: accountDialog
            parent: fullView
            switching: accountSwitching
            initialProviderId: navigation.selectedProviderId
            onWalletRequested: credentialStore.open()
            onConfigureRequested: Plasmoid.internalAction("configure").trigger()
        }
    }

    Component {
        id: overviewComponent

        OverviewPage {
            providers: backend.providers
            showIdleWindows: Plasmoid.configuration.showIdleWindows
            onProviderSelected: providerId => navigation.selectProvider(providerId)
        }
    }

    Component {
        id: providerComponent

        ProviderDetails {
            provider: navigation.selectedProvider
            switching: accountSwitching
            loading: backend.busy
            clockTick: root.clockTick
            showIdleWindows: Plasmoid.configuration.showIdleWindows
        }
    }

    Component.onCompleted: {
        credentialStore.open()
        backend.refresh()
    }
}
