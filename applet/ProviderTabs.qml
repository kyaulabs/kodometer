pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Item {
    id: root

    property var providers: []
    readonly property color overviewAccentColor: brand.logoColor

    BrandPalette {
        id: brand
        objectName: "tabBrandPalette"
    }
    property bool overviewVisible: providers.length > 1
    property int selectedIndex: overviewVisible ? 0 : -1
    readonly property var tabs: {
        const result = []
        if (overviewVisible) {
            result.push({
                            id: "",
                            name: "Overview",
                            accent: root.overviewAccentColor,
                            overview: true
                        })
        }
        for (const provider of providers) {
            result.push({
                            id: provider.id,
                            name: provider.name || provider.id,
                            accent: provider.display && provider.display.accentColor
                                    ? provider.display.accentColor : Kirigami.Theme.highlightColor,
                            overview: false
                        })
        }
        return result
    }
    readonly property int tabCount: tabs.length

    signal overviewSelected
    signal providerSelected(string providerId)

    function activateTab(index) {
        if (index < 0 || index >= tabs.length) {
            return
        }
        const tab = tabs[index]
        if (tab.overview) {
            overviewSelected()
        } else {
            providerSelected(tab.id)
        }
    }

    implicitHeight: Kirigami.Units.gridUnit * 3.5

    ListView {
        id: tabList

        anchors.fill: parent
        orientation: ListView.Horizontal
        spacing: Kirigami.Units.smallSpacing
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.tabs
        currentIndex: root.selectedIndex

        delegate: QQC2.TabButton {
            id: button
            objectName: "provider-tab-" + index

            required property int index
            required property var modelData
            width: Kirigami.Units.gridUnit * 4
            height: tabList.height
            topPadding: Kirigami.Units.smallSpacing
            bottomPadding: topPadding
            checked: index === root.selectedIndex
            palette.highlight: button.modelData.overview ? root.overviewAccentColor :
                                                           Kirigami.Theme.highlightColor
            onClicked: root.activateTab(index)

            Rectangle {
                objectName: "provider-tab-accent-" + button.index
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 1
                height: Kirigami.Units.smallSpacing
                radius: height / 2
                color: button.modelData.accent || Kirigami.Theme.highlightColor
            }

            contentItem: Item {
                Column {
                    objectName: "provider-tab-content-" + button.index
                    anchors.centerIn: parent
                    width: parent.width
                    spacing: 2

                    Item {
                        width: parent.width
                        height: Kirigami.Units.iconSizes.smallMedium

                        ProviderIcon {
                            anchors.centerIn: parent
                            width: Kirigami.Units.iconSizes.smallMedium
                            height: width
                            visible: !button.modelData.overview
                            providerId: button.modelData.id
                        }

                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: Kirigami.Units.iconSizes.smallMedium
                            height: width
                            visible: button.modelData.overview
                            source: Qt.resolvedUrl("assets/kodometer-symbolic.svg")
                            isMask: true
                            color: button.checked ? Kirigami.Theme.highlightedTextColor :
                                                    Kirigami.Theme.textColor
                        }
                    }

                    QQC2.Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: button.modelData.name
                        textFormat: Text.PlainText
                        font: Kirigami.Theme.smallFont
                        color: button.checked ? Kirigami.Theme.highlightedTextColor :
                                                Kirigami.Theme.textColor
                    }
                }
            }
        }

        QQC2.ScrollBar.horizontal: QQC2.ScrollBar {
            policy: QQC2.ScrollBar.AsNeeded
        }
    }
}
