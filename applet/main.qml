pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import plasma.applet.org.kyaulabs.codexbar as Private

PlasmoidItem {
    id: root

    property int refreshIntervalMinutes: 5

    function remainingFor(kind) {
        if (backend.providers.length === 0) {
            return 0
        }
        const windows = backend.providers[0].windows || []
        for (const window of windows) {
            if (window.kind === kind) {
                return Number(window.remainingPercent)
            }
        }
        return 0
    }

    switchWidth: Kirigami.Units.gridUnit * 19
    switchHeight: Kirigami.Units.gridUnit * 16
    toolTipMainText: "CodexBar"
    toolTipSubText: backend.error.length > 0 ? backend.error : backend.providers.length
                                               + " providers"

    Private.DashboardController {
        id: backend
    }

    Timer {
        interval: root.refreshIntervalMinutes * 60 * 1000
        repeat: true
        running: true
        onTriggered: backend.refresh()
    }

    Component.onCompleted: backend.refresh()

    compactRepresentation: MouseArea {
        id: compact
        activeFocusOnTab: true
        Accessible.name: root.toolTipMainText
        Accessible.description: root.toolTipSubText
        Accessible.role: Accessible.Button
        onClicked: root.expanded = !root.expanded

        CompactMeter {
            anchors.centerIn: parent
            width: Math.min(parent.width, Kirigami.Units.gridUnit * 1.25)
            height: Math.min(parent.height, Kirigami.Units.gridUnit)
            sessionRemaining: root.remainingFor("session")
            weeklyRemaining: root.remainingFor("weekly")
            opacity: backend.busy ? 0.55 : 1
        }
    }

    fullRepresentation: Item {
        implicitWidth: Kirigami.Units.gridUnit * 19
        implicitHeight: Kirigami.Units.gridUnit * 24

        ColumnLayout {
            anchors.fill: parent
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 2
                    text: "CodexBar"
                }

                QQC2.BusyIndicator {
                    visible: backend.busy
                    running: visible
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: width
                }

                QQC2.ToolButton {
                    icon.name: "view-refresh-symbolic"
                    text: "Refresh"
                    display: QQC2.AbstractButton.IconOnly
                    enabled: !backend.busy
                    onClicked: backend.refresh()
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                }
            }

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth

                ListView {
                    spacing: Kirigami.Units.smallSpacing
                    model: backend.providers
                    clip: true

                    delegate: ProviderCard {
                        required property var modelData
                        width: ListView.view.width
                        provider: modelData
                    }
                }
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
                visible: backend.error.length > 0
                type: Kirigami.MessageType.Error
                text: backend.error
            }
        }
    }
}
