pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import plasma.applet.org.kyaulabs.kodometer as Private

ColumnLayout {
    id: root

    required property string providerId
    property string region: ""
    readonly property int actionCount: actionController.actions.length

    visible: actionCount > 0 || actionController.error.length > 0
    spacing: Kirigami.Units.smallSpacing

    Private.ProviderActions {
        id: actionController
        providerId: root.providerId
        region: root.region
    }

    Flow {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        Repeater {
            model: actionController.actions

            delegate: QQC2.Button {
                id: actionButton
                required property var modelData
                objectName: "provider-action-" + modelData.id
                text: modelData.label
                icon.name: "internet-web-browser"
                Accessible.description: qsTr("Opens %1 in your browser").arg(String(modelData.url))
                QQC2.ToolTip.text: Accessible.description
                QQC2.ToolTip.visible: hovered
                onClicked: actionController.open(modelData.id)
            }
        }
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        visible: actionController.error.length > 0
        type: Kirigami.MessageType.Error
        text: actionController.error
    }
}
