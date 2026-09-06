pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import plasma.applet.org.kyaulabs.kodometer as Private

Kirigami.ScrollablePage {
    id: root

    title: qsTr("Provider credentials")

    readonly property list<var> credentials: [
        {
            "name": qsTr("DeepSeek API key"),
            "key": "DEEPSEEK_API_KEY"
        },
        {
            "name": qsTr("Kimi Code API key"),
            "key": "KIMI_CODE_API_KEY"
        },
        {
            "name": qsTr("OpenRouter API key"),
            "key": "OPENROUTER_API_KEY"
        },
        {
            "name": qsTr("OpenRouter Management API key"),
            "key": "OPENROUTER_MANAGEMENT_API_KEY"
        },
        {
            "name": qsTr("xAI Management API key"),
            "key": "XAI_MANAGEMENT_API_KEY"
        },
        {
            "name": qsTr("z.ai API key"),
            "key": "Z_AI_API_KEY"
        }
    ]

    Private.KWalletCredentialStore {
        id: credentialStore
    }

    Component.onCompleted: credentialStore.open()

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Information
            text: qsTr(
                      "These are Default credentials stored in KDE Wallet. Non-empty matching environment variables take precedence. Configure named DeepSeek and Kimi Code accounts on the KWallet accounts page.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: credentialStore.error.length > 0
            type: Kirigami.MessageType.Error
            text: credentialStore.error

            actions: [
                Kirigami.Action {
                    text: qsTr("Retry")
                    icon.name: "view-refresh"
                    onTriggered: credentialStore.open()
                }
            ]
        }

        QQC2.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: credentialStore.busy
            running: visible
        }

        Repeater {
            model: root.credentials

            delegate: ColumnLayout {
                id: credentialRow

                required property var modelData
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                readonly property bool configured: credentialStore.configuredKeys.includes(
                                                       modelData.key)

                QQC2.Label {
                    text: credentialRow.modelData.name
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true

                    QQC2.TextField {
                        id: secretField
                        Layout.fillWidth: true
                        enabled: credentialStore.ready
                        echoMode: TextInput.Password
                        inputMethodHints: Qt.ImhHiddenText | Qt.ImhSensitiveData
                        placeholderText: credentialRow.configured ? qsTr("Stored in KWallet") : qsTr(
                                                                        "Enter API key")
                        onAccepted: saveButton.clicked()
                    }

                    QQC2.Button {
                        id: saveButton
                        text: credentialRow.configured ? qsTr("Replace") : qsTr("Save")
                        enabled: credentialStore.ready && secretField.text.length > 0
                        onClicked: {
                            if (credentialStore.saveSecret(credentialRow.modelData.key,
                                                           secretField.text)) {
                                secretField.clear()
                            }
                        }
                    }

                    QQC2.Button {
                        text: qsTr("Remove")
                        enabled: credentialStore.ready && credentialRow.configured
                        onClicked: credentialStore.removeSecret(credentialRow.modelData.key)
                    }
                }
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
            text: qsTr(
                      "xAI still requires XAI_TEAM_ID. z.ai region, scope, organization, and project selectors remain environment-based settings.")
        }
    }
}
