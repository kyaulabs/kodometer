pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import plasma.applet.org.kyaulabs.kodometer as Private

ColumnLayout {
    id: root

    required property var cost
    property color accentColor: Kirigami.Theme.highlightColor
    property string selectedDate: ""
    readonly property var points: history.view.points || []
    readonly property int dayCount: points.length
    readonly property var selectedPoint: points.find(point => point.date === root.selectedDate)
                                         || points[points.length - 1] || ({})

    visible: history.view.available === true
    spacing: Kirigami.Units.smallSpacing

    Private.CostHistoryModel {
        id: history
        cost: root.cost
        days: range.currentIndex === 0 ? 7 : 30
    }

    Kirigami.Separator {
        Layout.fillWidth: true
    }

    RowLayout {
        Layout.fillWidth: true

        QQC2.Label {
            Layout.fillWidth: true
            text: qsTr("Daily spend (USD)")
            font.bold: true
            wrapMode: Text.WordWrap
        }

        QQC2.ComboBox {
            id: range
            objectName: "costHistoryRange"
            model: [qsTr("7 days"), qsTr("30 days")]
            currentIndex: 1
            Accessible.name: qsTr("Cost history range")
        }
    }

    QQC2.Label {
        Layout.fillWidth: true
        text: history.view.rangeLabel || ""
        wrapMode: Text.WordWrap
        color: Kirigami.Theme.textColor
        font: Kirigami.Theme.smallFont
    }

    QQC2.Label {
        Layout.fillWidth: true
        text: history.view.summary || ""
        wrapMode: Text.WordWrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 2

        Repeater {
            model: root.points

            delegate: QQC2.AbstractButton {
                id: bar
                required property var modelData
                readonly property bool reported: modelData.amount !== undefined && modelData.amount
                                                 !== null
                objectName: "history-day-" + modelData.date
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: Kirigami.Units.gridUnit * 4
                implicitWidth: 0
                padding: 0
                hoverEnabled: true
                Accessible.name: modelData.description
                QQC2.ToolTip.visible: hovered || activeFocus
                QQC2.ToolTip.text: modelData.description
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: root.selectedDate = modelData.date

                background: Rectangle {
                    color: bar.hovered || bar.activeFocus ? Kirigami.Theme.alternateBackgroundColor :
                                                            "transparent"
                    border.width: bar.activeFocus ? 1 : 0
                    border.color: root.accentColor
                    radius: 2
                }

                contentItem: Item {
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: bar.reported ? Math.max(bar.modelData.amount > 0 ? 3 : 2, parent.height
                                                        * bar.modelData.fraction) : 1
                        color: bar.reported ? root.accentColor : Kirigami.Theme.disabledTextColor
                        radius: 1
                    }
                }
            }
        }
    }

    QQC2.Label {
        objectName: "costHistorySelection"
        Layout.fillWidth: true
        text: root.selectedPoint.description || ""
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
    }

    QQC2.Label {
        objectName: "costHistoryPartial"
        Layout.fillWidth: true
        visible: history.view.partial === true
        text: qsTr("Partial history; missing days are not counted as zero.")
        wrapMode: Text.WordWrap
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.textColor
    }

    QQC2.Label {
        objectName: "costHistoryEstimated"
        Layout.fillWidth: true
        visible: history.view.estimated === true
        text: qsTr("Provider history includes estimated spend.")
        wrapMode: Text.WordWrap
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.textColor
    }

    QQC2.Label {
        objectName: "costHistoryCurrentDay"
        Layout.fillWidth: true
        visible: history.view.includesCurrentDay === true
        text: qsTr("Includes an incomplete UTC day.")
        wrapMode: Text.WordWrap
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.textColor
    }
}
