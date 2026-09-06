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
    Plasmoid.icon: "view-statistics"
    Plasmoid.status: backend.busy ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus

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
                remaining = remaining === null ? value : Math.min(remaining, value)
            }
        }
        return remaining === null ? 0 : remaining
    }

    Private.KWalletCredentialStore {
        id: credentialStore
    }

    Private.UsageController {
        id: backend
        credentialStore: credentialStore
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

    Private.ProviderSelectionModel {
        id: navigation
        providers: backend.providers
    }

    Timer {
        interval: 60000
        repeat: true
        running: root.expanded
        onTriggered: root.clockTick++
    }

    compactRepresentation: Item {
        CompactMeter {
            anchors.fill: parent
            sessionRemaining: root.remainingFor("session")
            weeklyRemaining: root.remainingFor("weekly")
        }

        QQC2.ToolTip.visible: compactMouse.containsMouse
        QQC2.ToolTip.text: qsTr("Kodometer usage")

        MouseArea {
            id: compactMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.expanded = !root.expanded
        }
    }

    fullRepresentation: Item {
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

            ProviderTabs {
                Layout.fillWidth: true
                visible: providers.length > 0
                providers: backend.providers
                overviewVisible: providers.length > 1
                selectedIndex: navigation.selectedTabIndex
                onOverviewSelected: navigation.selectOverview()
                onProviderSelected: providerId => navigation.selectProvider(providerId)
            }

            Kirigami.Separator {
                Layout.fillWidth: true
                visible: backend.providers.length > 0
            }

            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                active: backend.providers.length > 0
                sourceComponent: navigation.overviewSelected ? overviewComponent : providerComponent
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: backend.providers.length === 0

                Item {
                    Layout.fillHeight: true
                }
                Kirigami.Icon {
                    Layout.alignment: Qt.AlignHCenter
                    source: backend.busy ? "view-refresh" : "view-statistics"
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
                visible: backend.error.length > 0
                type: Kirigami.MessageType.Error
                text: backend.error
            }

            RowLayout {
                Layout.fillWidth: true

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
                    onClicked: backend.refresh()
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                }
            }
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
            clockTick: root.clockTick
            showIdleWindows: Plasmoid.configuration.showIdleWindows
        }
    }

    Component.onCompleted: {
        credentialStore.open()
        backend.refresh()
    }
}
