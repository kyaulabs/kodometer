pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root

    required property var accountStore
    required property string providerId
    required property string providerName
    property string selectedId: ""
    readonly property bool pairedKeys: providerId === "openrouter"
    readonly property bool teamAccount: providerId === "xai"
    readonly property bool regionalAccount: providerId === "zai"
    readonly property bool completeCredentials: secret.text.length > 0 && (!teamAccount
                                                                           || team.text.length > 0)
                                                && (!regionalAccount || (region.currentIndex >= 0
                                                                         && usageScope.currentIndex
                                                                         >= 0 && (usageScope.currentIndex
                                                                                  === 0 || (
                                                                                      organization.text.length
                                                                                      > 0 && project.text.length
                                                                                      > 0))))
    readonly property var zaiOptions: {
        if (!regionalAccount)
            return {}
        const options = {
            region: region.currentIndex === 1 ? "bigmodel-cn" : "global",
            scope: usageScope.currentIndex === 1 ? "team" : "personal"
        }
        if (usageScope.currentIndex === 1) {
            options.organizationId = organization.text.trim()
            options.projectId = project.text.trim()
        }
        return options
    }
    readonly property var entries: accountStore.providers[providerId] || []
    readonly property bool available: entries.some(entry => entry.id === root.selectedId)
    readonly property var choices: {
        const items = [
                  {
                      id: "",
                      name: qsTr("Default (existing credentials)")
                  }
              ].concat(root.entries)
        if (root.selectedId.length > 0 && !root.available)
            items.push({
                           id: root.selectedId,
                           name: qsTr("Selected account unavailable")
                       })
        return items
    }

    function clearCredentials() {
        if (secret)
            secret.clear()
        if (management)
            management.clear()
        if (team)
            team.clear()
        if (region)
            region.currentIndex = -1
        if (usageScope)
            usageScope.currentIndex = -1
        if (organization)
            organization.clear()
        if (project)
            project.clear()
    }

    onSelectedIdChanged: clearCredentials()

    Connections {
        target: root.accountStore
        function onChanged() {
            if (!root.accountStore.ready)
                root.clearCredentials()
        }
    }

    QQC2.Label {
        text: root.providerName
        font.bold: true
    }

    QQC2.ComboBox {
        id: selector
        objectName: root.providerId + "-wallet-selector"
        Layout.fillWidth: true
        model: root.choices
        textRole: "name"
        valueRole: "id"
        currentIndex: root.choices.findIndex(entry => entry.id === root.selectedId)
        Accessible.name: qsTr("%1 account").arg(root.providerName)
        onActivated: root.selectedId = currentValue
        // Preserve native ComboBox text painting; only popup labels need a plain-text override.
        delegate: QQC2.ItemDelegate {
            id: choice
            required property var modelData
            required property int index
            width: selector.width
            text: modelData.name
            highlighted: selector.highlightedIndex === index
            contentItem: QQC2.Label {
                text: choice.text
                textFormat: Text.PlainText
                elide: Text.ElideRight
            }
        }
    }

    QQC2.TextField {
        id: name
        objectName: root.providerId + "-wallet-name"
        Layout.fillWidth: true
        enabled: root.accountStore.ready
        maximumLength: 64
        placeholderText: qsTr("Name for a new account")
        Accessible.name: qsTr("New %1 account name").arg(root.providerName)
    }

    QQC2.TextField {
        id: secret
        objectName: root.providerId + "-wallet-key"
        Layout.fillWidth: true
        enabled: root.accountStore.ready
        maximumLength: 65536
        echoMode: TextInput.Password
        inputMethodHints: Qt.ImhHiddenText | Qt.ImhSensitiveData
        placeholderText: root.teamAccount ? qsTr("Enter an xAI Management API key") : qsTr(
                                                "Enter a new or replacement API key")
        Accessible.name: root.teamAccount ? qsTr("xAI Management API key") : qsTr("%1 API key").arg(
                                                root.providerName)
    }

    QQC2.TextField {
        id: management
        objectName: root.providerId + "-wallet-management-key"
        Layout.fillWidth: true
        visible: root.pairedKeys
        enabled: root.accountStore.ready
        maximumLength: 65536
        echoMode: TextInput.Password
        inputMethodHints: Qt.ImhHiddenText | Qt.ImhSensitiveData
        placeholderText: qsTr("Optional Management API key for Activity")
        Accessible.name: qsTr("OpenRouter Management API key")
    }

    QQC2.Label {
        Layout.fillWidth: true
        visible: root.pairedKeys
        wrapMode: Text.WordWrap
        text: qsTr(
                  "Re-enter both keys to replace the pair. Leaving Management blank removes it and disables Activity for this account.")
    }

    QQC2.TextField {
        id: team
        objectName: root.providerId + "-wallet-team"
        Layout.fillWidth: true
        visible: root.teamAccount
        enabled: root.accountStore.ready
        maximumLength: 256
        placeholderText: qsTr("Required xAI team ID")
        Accessible.name: qsTr("xAI team ID")
    }

    QQC2.Label {
        Layout.fillWidth: true
        visible: root.teamAccount
        wrapMode: Text.WordWrap
        text: qsTr(
                  "Re-enter the Management key and team ID to replace this account. Team IDs use 1–256 ASCII letters, digits, underscores, or hyphens. Neither value falls back to the environment.")
    }

    QQC2.ComboBox {
        id: region
        objectName: root.providerId + "-wallet-region"
        Layout.fillWidth: true
        visible: root.regionalAccount
        enabled: root.accountStore.ready
        model: [qsTr("Global"), qsTr("BigModel CN")]
        currentIndex: -1
        displayText: currentIndex < 0 ? qsTr("Choose z.ai region") : currentText
        Accessible.name: qsTr("z.ai region")
    }

    QQC2.ComboBox {
        id: usageScope
        objectName: root.providerId + "-wallet-scope"
        Layout.fillWidth: true
        visible: root.regionalAccount
        enabled: root.accountStore.ready
        model: [qsTr("Personal"), qsTr("Team")]
        currentIndex: -1
        displayText: currentIndex < 0 ? qsTr("Choose z.ai usage scope") : currentText
        Accessible.name: qsTr("z.ai usage scope")
        onActivated: {
            organization.clear()
            project.clear()
        }
    }

    QQC2.TextField {
        id: organization
        objectName: root.providerId + "-wallet-organization"
        Layout.fillWidth: true
        visible: root.regionalAccount && usageScope.currentIndex === 1
        enabled: root.accountStore.ready
        maximumLength: 256
        placeholderText: qsTr("Required organization ID")
        Accessible.name: qsTr("z.ai organization ID")
    }

    QQC2.TextField {
        id: project
        objectName: root.providerId + "-wallet-project"
        Layout.fillWidth: true
        visible: root.regionalAccount && usageScope.currentIndex === 1
        enabled: root.accountStore.ready
        maximumLength: 256
        placeholderText: qsTr("Required project ID")
        Accessible.name: qsTr("z.ai project ID")
    }

    QQC2.Label {
        Layout.fillWidth: true
        visible: root.regionalAccount
        wrapMode: Text.WordWrap
        text: qsTr(
                  "Re-enter the key and choose its region and scope for every save. Team scope requires organization and project IDs of 1–256 ASCII letters, digits, underscores, or hyphens. No value falls back to environment settings or credential files.")
    }

    Flow {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: Kirigami.Units.smallSpacing

        QQC2.Button {
            objectName: root.providerId + "-wallet-add"
            text: qsTr("Add account")
            enabled: root.accountStore.ready && name.text.length > 0 && root.completeCredentials
            onClicked: {
                const id = root.accountStore.addAccount(root.providerId, name.text, secret.text,
                                                        management.text, team.text, root.zaiOptions)
                if (id.length > 0) {
                    root.selectedId = id
                    name.clear()
                    root.clearCredentials()
                }
            }
        }
        QQC2.Button {
            objectName: root.providerId + "-wallet-replace"
            text: root.regionalAccount ? qsTr("Replace key and settings") : root.teamAccount ? qsTr(
                                                                                                   "Replace key and team") :
                                                                                               root.pairedKeys
                                                                                               ? qsTr("Replace selected keys") :
                                                                                                 qsTr("Replace selected key")
            enabled: root.accountStore.ready && root.available && root.completeCredentials
            onClicked: {
                if (root.accountStore.replaceAccount(root.providerId, root.selectedId, secret.text,
                                                     management.text, team.text, root.zaiOptions))
                    root.clearCredentials()
            }
        }
        QQC2.Button {
            objectName: root.providerId + "-wallet-remove"
            text: qsTr("Remove selected account")
            enabled: root.accountStore.ready && root.available
            onClicked: root.accountStore.removeAccount(root.providerId, root.selectedId)
        }
    }
}
