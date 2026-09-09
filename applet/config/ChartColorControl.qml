pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: root

    property string colorValue
    property color fallbackColor: Kirigami.Theme.highlightColor
    readonly property bool customColor: /^#[0-9a-fA-F]{6}$/.test(colorValue)

    Rectangle {
        implicitWidth: Kirigami.Units.gridUnit
        implicitHeight: implicitWidth
        color: root.customColor ? root.colorValue : root.fallbackColor
        border.color: Kirigami.Theme.textColor
        radius: 3
    }

    QQC2.Button {
        objectName: "chooseColor"
        text: qsTr("Choose color…")
        icon.name: "color-picker"
        onClicked: {
            picker.selectedColor = root.customColor ? root.colorValue : root.fallbackColor
            picker.open()
        }
    }

    QQC2.Button {
        objectName: "resetColor"
        text: qsTr("Use provider color")
        enabled: root.colorValue !== ""
        onClicked: root.colorValue = ""
    }

    ColorDialog {
        id: picker
        objectName: "colorPicker"
        title: qsTr("Donut color")
        onAccepted: root.colorValue = selectedColor.toString()
    }
}
