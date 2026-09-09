pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: root
    required property var navigation
    property var switching: null
    property var quotaHistory: null
    property bool busy: false
    property bool showIdleWindows: false
    property bool fillRemaining: true
    property int clockTick: 0
    property string generatedAt
    property string error
    property string profileError
    property real availableHeight: Screen.height
    signal refreshRequested
    signal configureRequested
    signal walletRequested

    readonly property real chromeHeight: {
        let total = Kirigami.Units.largeSpacing * 2
        let count = 0
        for (const child of contentLayout.children) {
            if (child === pageLoader || !child.visible)
                continue
            total += child.implicitHeight + child.Layout.topMargin + child.Layout.bottomMargin
            count++
        }
        return total + Math.max(0, count) * contentLayout.spacing
    }
    readonly property real pageHeightLimit: Math.max(1, availableHeight * 0.85 - chromeHeight)

    implicitWidth: Kirigami.Units.gridUnit * 22
    implicitHeight: Math.min(contentLayout.implicitHeight + Kirigami.Units.largeSpacing * 2,
                             availableHeight * 0.85)
    Layout.preferredHeight: implicitHeight
    Layout.minimumWidth: Kirigami.Units.gridUnit * 18
    Layout.minimumHeight: implicitHeight
    Layout.maximumHeight: implicitHeight
    focus: true
    Keys.onLeftPressed: navigation.selectPrevious()
    Keys.onRightPressed: navigation.selectNext()

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        BrandPalette {
            id: brand
        }

        Image {
            objectName: "headerWordmark"
            Layout.topMargin: -Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
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
            objectName: "popupProviderTabs"
            Layout.fillWidth: true
            visible: providers.length > 0
            providers: root.navigation.displayedProviders
            overviewVisible: providers.length > 1
            selectedIndex: root.navigation.selectedTabIndex
            onOverviewSelected: root.navigation.selectOverview()
            onProviderSelected: providerId => root.navigation.selectProvider(providerId)
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            visible: root.navigation.displayedProviders.length > 0
        }

        Loader {
            id: pageLoader
            objectName: "popupPageLoader"
            readonly property var page: item
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: page ? page.contentHeight : 0
            active: root.navigation.displayedProviders.length > 0
            sourceComponent: root.navigation.overviewSelected ? overviewComponent :
                                                                providerComponent
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 10
            visible: root.navigation.displayedProviders.length === 0
            Item {
                Layout.fillHeight: true
            }
            Kirigami.Icon {
                Layout.alignment: Qt.AlignHCenter
                source: root.busy ? "view-refresh" : Qt.resolvedUrl("assets/kodometer-symbolic.svg")
                isMask: !root.busy
                implicitWidth: Kirigami.Units.iconSizes.large
                implicitHeight: width
            }
            QQC2.Label {
                Layout.alignment: Qt.AlignHCenter
                text: root.busy ? qsTr("Loading provider usage…") : qsTr("No provider data")
            }
            QQC2.Button {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Configure providers…")
                onClicked: root.configureRequested()
            }
            Item {
                Layout.fillHeight: true
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.profileError.length > 0
            type: Kirigami.MessageType.Error
            text: root.profileError
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.error.length > 0
            type: Kirigami.MessageType.Error
            text: root.error
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.switching && root.switching.error.length > 0
            type: Kirigami.MessageType.Error
            text: root.switching ? root.switching.error : ""
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.quotaHistory && root.quotaHistory.error.length > 0
            text: root.quotaHistory ? root.quotaHistory.error : ""
            type: Kirigami.MessageType.Warning
        }

        RowLayout {
            objectName: "popupFooter"
            Layout.fillWidth: true
            QQC2.ToolButton {
                objectName: "configureAccounts"
                icon.name: "configure"
                text: qsTr("Configure providers and accounts…")
                display: QQC2.AbstractButton.IconOnly
                onClicked: root.configureRequested()
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
            }
            QQC2.ToolButton {
                objectName: "clearQuotaHistory"
                icon.name: "edit-clear-history"
                text: qsTr("Clear quota history…")
                display: QQC2.AbstractButton.IconOnly
                visible: root.quotaHistory !== null
                onClicked: clearHistoryDialog.open()
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
            }
            QQC2.BusyIndicator {
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: width
                running: root.busy
                visible: running
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: root.generatedAt ? qsTr("Snapshot %1").arg(root.generatedAt) : ""
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
            }
            QQC2.ToolButton {
                icon.name: "view-refresh"
                text: qsTr("Refresh")
                display: QQC2.AbstractButton.IconOnly
                enabled: !root.busy
                onClicked: root.refreshRequested()
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
            }
        }
    }

    QQC2.Dialog {
        id: clearHistoryDialog
        objectName: "clearHistoryConfirmation"
        parent: root
        anchors.centerIn: parent
        title: qsTr("Clear all saved quota history?")
        modal: true
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
        onAccepted: {
            if (root.quotaHistory)
                root.quotaHistory.clear()
        }
        QQC2.Label {
            text: qsTr(
                      "Deletes recorded quota history for all providers, profiles, and accounts. Credentials, preferences, and provider-reported billing history are unchanged. Later successful refreshes start new observations.")
            width: Math.max(0, Math.min(root.width - Kirigami.Units.gridUnit * 4,
                                        Kirigami.Units.gridUnit * 18))
            wrapMode: Text.WordWrap
        }
    }

    Component {
        id: overviewComponent
        OverviewPage {
            maximumContentHeight: root.pageHeightLimit
            providers: root.navigation.displayedProviders
            showIdleWindows: root.showIdleWindows
            fillRemaining: root.fillRemaining
            onProviderSelected: providerId => root.navigation.selectProvider(providerId)
        }
    }
    Component {
        id: providerComponent
        ProviderDetails {
            maximumContentHeight: root.pageHeightLimit
            provider: root.navigation.selectedProvider
            switching: root.switching
            quotaHistory: root.quotaHistory
            loading: root.busy
            clockTick: root.clockTick
            showIdleWindows: root.showIdleWindows
            fillRemaining: root.fillRemaining
        }
    }
}
