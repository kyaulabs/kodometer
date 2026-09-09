pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

PaginatedPage {
    id: root

    property var providers: []
    property bool showIdleWindows: false
    property bool fillRemaining: true
    readonly property int visibleProviderCount: providers.length

    signal providerSelected(string providerId)

    contentItem: ColumnLayout {
        id: overview
        objectName: "pageContent"

        parent: root.contentHost
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
                objectName: "overview-provider-" + modelData.id

                Layout.fillWidth: true
                provider: modelData
                showIdleWindows: root.showIdleWindows
                fillRemaining: root.fillRemaining
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
}
