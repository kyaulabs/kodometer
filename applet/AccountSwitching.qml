pragma ComponentBehavior: Bound

import QtQuick
import plasma.applet.org.kyaulabs.kodometer as Private

QtObject {
    id: root

    required property var configuration
    property var accounts: null
    readonly property var providers: model.providers
    property string error: ""
    signal selectionApplied(string providerId)

    readonly property Private.AccountSwitchModel model: Private.AccountSwitchModel {
        accounts: root.accounts
        oauthConfiguration: root.configuration.oauthProfiles
        walletSelections: ({
                               deepseek: root.configuration.deepseekAccountId,
                               kimi: root.configuration.kimiAccountId,
                               openrouter: root.configuration.openrouterAccountId,
                               xai: root.configuration.xaiAccountId,
                               zai: root.configuration.zaiAccountId
                           })
        disabledProviders: root.configuration.disabledProviders
    }

    function select(providerId, accountId) {
        const change = model.selectionChange(providerId, accountId)
        if (!change.key) {
            error = qsTr("This selection is unavailable. Refresh the wallet or configure profiles.")
            return false
        }
        error = ""
        // Write the configuration source, never the controller's bound properties.
        if (configuration[change.key] !== change.value) {
            configuration[change.key] = change.value
            configuration.writeConfig()
        }
        selectionApplied(providerId)
        return true
    }
}
