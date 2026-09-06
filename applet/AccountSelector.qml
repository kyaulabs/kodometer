pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

RowLayout {
    id: root

    property var switching: null
    property string providerId: ""
    readonly property var view: switching ? switching.providers.find(row => row.id
                                                                            === root.providerId) || (
                                                {}) : ({})
    readonly property var choices: view.choices || []
    visible: Boolean(view.id)

    QQC2.Label {
        text: root.view.kind === "profile" ? qsTr("Profile") : qsTr("Account")
    }

    QQC2.ComboBox {
        id: selector
        objectName: "runtime-account-selector"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        enabled: Boolean(root.view.enabled)
        model: root.choices
        textRole: "name"
        valueRole: "id"
        currentIndex: root.choices.findIndex(row => row.id === root.view.selectedId)
        Accessible.name: qsTr("%1 %2").arg(root.view.name || "").arg(root.view.kind === "profile" ? qsTr(
                                                                                                        "profile") :
                                                                                                    qsTr("account"))
        onActivated: {
            root.switching.select(root.providerId, currentValue)
            selector.currentIndex = Qt.binding(function () {
                return root.choices.findIndex(row => row.id === root.view.selectedId)
            })
        }

        contentItem: QQC2.Label {
            text: selector.displayText
            textFormat: Text.PlainText
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        delegate: QQC2.ItemDelegate {
            id: choice
            required property var modelData
            required property int index
            width: selector.width
            enabled: modelData.available
            highlighted: selector.highlightedIndex === index
            contentItem: QQC2.Label {
                text: choice.modelData.name
                textFormat: Text.PlainText
                elide: Text.ElideRight
            }
        }
        QQC2.ToolTip.text: qsTr("Save this widget's selection immediately.")
        QQC2.ToolTip.visible: hovered
    }
}
