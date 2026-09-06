pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.Dialog {
    id: root

    required property var switching
    property string initialProviderId: ""
    property string targetProviderId: ""
    signal walletRequested
    signal configureRequested

    title: qsTr("Switch account")
    modal: true
    focus: true
    standardButtons: QQC2.Dialog.Close
    width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, Kirigami.Units.gridUnit * 22)
    anchors.centerIn: parent

    onOpened: {
        const choices = switching.providers
        targetProviderId = choices.some(row => row.id === initialProviderId) ? initialProviderId : (
                                                                                   choices.length
                                                                                   > 0 ? choices[0].id :
                                                                                         "")
    }
    Connections {
        target: root.switching
        function onSelectionApplied(providerId) {
            root.close()
        }
        function onProvidersChanged() {
            const choices = root.switching.providers
            if (!choices.some(row => row.id === root.targetProviderId))
                root.targetProviderId = choices.length > 0 ? choices[0].id : ""
        }
    }

    contentItem: ColumnLayout {
        QQC2.Label {
            Layout.fillWidth: true
            text: qsTr(
                      "Switching saves only this widget's selection. Normal authentication and refresh rules still apply.")
            wrapMode: Text.WordWrap
        }
        QQC2.ComboBox {
            id: providerSelector
            objectName: "switch-provider-selector"
            Layout.fillWidth: true
            model: root.switching.providers
            textRole: "name"
            valueRole: "id"
            currentIndex: root.switching.providers.findIndex(row => row.id
                                                                    === root.targetProviderId)
            Accessible.name: qsTr("Provider to switch")
            onActivated: root.targetProviderId = currentValue
        }
        AccountSelector {
            Layout.fillWidth: true
            switching: root.switching
            providerId: root.targetProviderId
        }
        QQC2.Label {
            Layout.fillWidth: true
            visible: root.switching.error.length > 0
            text: root.switching.error
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
        }
        QQC2.Button {
            objectName: "switch-wallet-retry"
            text: qsTr("Open / retry KWallet")
            onClicked: root.walletRequested()
        }
        QQC2.Button {
            objectName: "switch-configure"
            text: qsTr("Configure profiles and accounts…")
            onClicked: {
                root.close()
                root.configureRequested()
            }
        }
    }
}
