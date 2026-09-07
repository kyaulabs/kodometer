pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root

    required property var section

    spacing: Kirigami.Units.smallSpacing

    Kirigami.Separator {
        Layout.fillWidth: true
    }

    QQC2.Label {
        text: root.section.title || qsTr("Details")
        font.bold: true
        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.08
    }

    Repeater {
        model: root.section.rows || []

        delegate: ColumnLayout {
            id: detailRow

            required property var modelData

            Layout.fillWidth: true
            spacing: 0

            RowLayout {
                Layout.fillWidth: true

                QQC2.Label {
                    Layout.fillWidth: true
                    text: detailRow.modelData.label || ""
                    elide: Text.ElideRight
                }
                QQC2.Label {
                    text: detailRow.modelData.value || ""
                    color: Kirigami.Theme.disabledTextColor
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                visible: Boolean(detailRow.modelData.secondaryValue)
                text: detailRow.modelData.secondaryValue || ""
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
            }
        }
    }
}
