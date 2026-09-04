pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Item {
    id: root

    property var providers: []
    property bool overviewVisible: providers.length > 1
    property int selectedIndex: overviewVisible ? 0 : -1
    readonly property var tabs: {
        const result = []
        if (overviewVisible) {
            result.push({
                            id: "",
                            name: "Overview",
                            overview: true
                        })
        }
        for (const provider of providers) {
            result.push({
                            id: provider.id,
                            name: provider.name || provider.id,
                            color: provider.display && provider.display.accentColor
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

            required property int index
            required property var modelData
            width: Kirigami.Units.gridUnit * 4
            height: tabList.height
            checked: index === root.selectedIndex
            onClicked: root.activateTab(index)

            contentItem: Column {
                spacing: 2

                Item {
                    width: parent.width
                    height: Kirigami.Units.iconSizes.smallMedium

                    Rectangle {
                        anchors.centerIn: parent
                        width: Kirigami.Units.iconSizes.small
                        height: width
                        radius: width / 2
                        color: button.modelData.overview ? Kirigami.Theme.highlightColor :
                                                           button.modelData.color

                        QQC2.Label {
                            anchors.centerIn: parent
                            text: button.modelData.overview ? "●" : button.modelData.name.charAt(0).toUpperCase(
                                                                  )

                            color: "white"
                            font.bold: true
                        }
                    }
                }

                QQC2.Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: button.modelData.name
                    font: Kirigami.Theme.smallFont
                    color: button.checked ? Kirigami.Theme.highlightedTextColor :
                                            Kirigami.Theme.textColor
                }
            }
        }

        QQC2.ScrollBar.horizontal: QQC2.ScrollBar {
            policy: QQC2.ScrollBar.AsNeeded
        }
    }
}
