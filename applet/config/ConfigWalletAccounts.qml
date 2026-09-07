pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import plasma.applet.org.kyaulabs.kodometer as Private

Kirigami.ScrollablePage {
    id: root

    title: qsTr("KWallet accounts")

    property alias cfg_deepseekAccountId: deepseek.selectedId
    property alias cfg_kimiAccountId: kimi.selectedId
    property alias cfg_openrouterAccountId: openrouter.selectedId
    property alias cfg_xaiAccountId: xai.selectedId
    property alias cfg_zaiAccountId: zai.selectedId
    // Injectable for offscreen tests; the real store never reveals saved keys to QML.
    property var accountStore: wallet.accounts

    Private.KWalletCredentialStore {
        id: wallet
    }

    Component.onCompleted: {
        if (accountStore === wallet.accounts)
            wallet.open()
    }

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr(
                      "Choose one account per provider. Named accounts use only their selected KWallet keys, without environment or CLI fallback. Default keeps the existing credential discovery.")
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr(
                      "Account saves and removals take effect immediately and are shared by all widgets. Cancel does not undo them. Only the widget's account selections use Apply/Cancel. Removing an account pauses widgets using it until another account or Default is selected.")
        }
        QQC2.Label {
            Layout.fillWidth: true
            visible: root.accountStore.error.length > 0
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            text: root.accountStore.error
            color: Kirigami.Theme.negativeTextColor
        }
        QQC2.Button {
            text: qsTr("Open / retry KWallet")
            enabled: !wallet.busy && root.accountStore === wallet.accounts
            onClicked: wallet.open()
        }
        WalletAccountEditor {
            id: deepseek
            Layout.fillWidth: true
            accountStore: root.accountStore
            providerId: "deepseek"
            providerName: "DeepSeek"
        }
        Kirigami.Separator {
            Layout.fillWidth: true
        }
        WalletAccountEditor {
            id: kimi
            Layout.fillWidth: true
            accountStore: root.accountStore
            providerId: "kimi"
            providerName: "Kimi Code"
        }
        Kirigami.Separator {
            Layout.fillWidth: true
        }
        WalletAccountEditor {
            id: openrouter
            Layout.fillWidth: true
            accountStore: root.accountStore
            providerId: "openrouter"
            providerName: "OpenRouter"
        }
        Kirigami.Separator {
            Layout.fillWidth: true
        }
        WalletAccountEditor {
            id: xai
            Layout.fillWidth: true
            accountStore: root.accountStore
            providerId: "xai"
            providerName: "xAI"
        }
        Kirigami.Separator {
            Layout.fillWidth: true
        }
        WalletAccountEditor {
            id: zai
            Layout.fillWidth: true
            accountStore: root.accountStore
            providerId: "zai"
            providerName: "z.ai"
        }
    }
}
