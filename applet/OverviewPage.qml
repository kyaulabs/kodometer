pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Flickable {
    id: root

    property var providers: []
    property bool showIdleWindows: false
    readonly property int visibleProviderCount: Math.min(6, providers.length)

    signal providerSelected(string providerId)

    contentWidth: width
    contentHeight: overview.implicitHeight + Kirigami.Units.largeSpacing
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: overview

        width: root.width
        spacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            Layout.fillWidth: true
            text: qsTr("Overview")
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.35
            font.bold: true
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: qsTr("Quota across %n provider(s)", "", root.providers.length)
            color: Kirigami.Theme.disabledTextColor
        }

        Repeater {
            model: root.providers.slice(0, root.visibleProviderCount)

            delegate: ProviderSummary {
                required property var modelData

                Layout.fillWidth: true
                provider: modelData
                showIdleWindows: root.showIdleWindows
                onClicked: root.providerSelected(modelData.id)
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            visible: root.providers.length > root.visibleProviderCount
            text: qsTr("%1 more providers available in the tabs").arg(root.providers.length
                                                                      - root.visibleProviderCount)
            color: Kirigami.Theme.disabledTextColor
            horizontalAlignment: Text.AlignHCenter
            font: Kirigami.Theme.smallFont
        }
    }

    QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
}
