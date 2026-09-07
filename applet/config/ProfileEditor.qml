pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root

    required property var profileModel
    required property string providerId
    required property string providerName
    readonly property var profileData: profileModel.providers[providerId] || ({})
    readonly property var entries: profileData.entries || []
    readonly property var selected: entries.find(entry => entry.id === root.profileData.selectedId)
                                    || ({})

    spacing: Kirigami.Units.smallSpacing

    QQC2.Label {
        text: root.providerName
        font.bold: true
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.ComboBox {
            id: selector
            objectName: root.providerId + "-profile-selector"
            Layout.fillWidth: true
            model: root.entries
            textRole: "name"
            valueRole: "id"
            currentIndex: root.entries.findIndex(entry => entry.id === root.profileData.selectedId)
            enabled: root.profileModel.valid
            Accessible.name: qsTr("%1 profile").arg(root.providerName)
            onActivated: root.profileModel.selectProfile(root.providerId, currentValue)

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

        QQC2.Button {
            objectName: root.providerId + "-profile-remove"
            text: qsTr("Remove")
            enabled: root.profileModel.valid && root.profileData.selectedId !== "default"
            onClicked: root.profileModel.removeProfile(root.providerId, root.profileData.selectedId)
        }
    }

    QQC2.Label {
        Layout.fillWidth: true
        text: root.selected.directory || qsTr(
                  "Use environment overrides and the standard credential location.")
        textFormat: Text.PlainText
        wrapMode: Text.WrapAnywhere
    }

    QQC2.TextField {
        id: name
        objectName: root.providerId + "-profile-name"
        Layout.fillWidth: true
        placeholderText: qsTr("Profile name, such as Work")
        maximumLength: 64
        Accessible.name: qsTr("New %1 profile name").arg(root.providerName)
        enabled: root.profileModel.valid
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.TextField {
            id: directory
            objectName: root.providerId + "-profile-directory"
            Layout.fillWidth: true
            placeholderText: qsTr("Absolute credential folder path")
            maximumLength: 4096
            Accessible.name: qsTr("New %1 credential folder").arg(root.providerName)
            enabled: root.profileModel.valid
        }

        QQC2.Button {
            text: qsTr("Browse…")
            enabled: root.profileModel.valid
            onClicked: folderDialog.open()
        }
    }

    QQC2.Button {
        objectName: root.providerId + "-profile-add"
        text: qsTr("Add and select")
        enabled: root.profileModel.valid
        onClicked: {
            if (root.profileModel.addProfile(root.providerId, name.text, directory.text)) {
                name.clear()
                directory.clear()
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose %1 credential folder").arg(root.providerName)
        onAccepted: directory.text = root.profileModel.localDirectory(selectedFolder)
    }
}
