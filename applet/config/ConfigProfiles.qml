pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import plasma.applet.org.kyaulabs.kodometer as Private

Kirigami.ScrollablePage {
    id: root
    title: qsTr("OAuth profiles")

    property alias cfg_oauthProfiles: profiles.configuration

    Private.OAuthProfiles {
        id: profiles
        objectName: "oauthProfileEditor"
    }

    ColumnLayout {
        QQC2.Label {
            Layout.fillWidth: true
            text: qsTr(
                      "Only the selected profile is refreshed. Choose existing folders containing Codex auth.json, Claude .credentials.json, or Gemini oauth_creds.json (with settings.json when present). Kodometer does not sign in or copy credentials.")
            wrapMode: Text.WordWrap
        }

        ProfileEditor {
            Layout.fillWidth: true
            profileModel: profiles
            providerId: "codex"
            providerName: "Codex"
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        ProfileEditor {
            Layout.fillWidth: true
            profileModel: profiles
            providerId: "claude"
            providerName: "Claude"
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        ProfileEditor {
            Layout.fillWidth: true
            profileModel: profiles
            providerId: "gemini"
            providerName: "Gemini"
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: qsTr(
                      "Apply saves profile names, folder paths, and selections—not secrets. Cancel discards edits. Removing a selected profile restores Default without deleting files. OAuth renewal may update the selected credential file, but selection does not change the CLI's active login.")
            wrapMode: Text.WordWrap
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: profiles.error.length > 0
            text: profiles.error
            type: Kirigami.MessageType.Error
        }
    }
}
