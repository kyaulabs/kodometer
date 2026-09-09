pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: root

    required property Item contentItem
    property real maximumContentHeight: 100000
    property int currentPage: 0
    readonly property alias contentHost: viewport
    readonly property real navigationHeight: pageTabs.implicitHeight + Kirigami.Units.smallSpacing
    readonly property var pages: {
        const total = contentItem.implicitHeight
        if (total <= maximumContentHeight)
            return [
                        {
                            start: 0,
                            end: total
                        }
                    ]
        const capacity = Math.max(1, maximumContentHeight - navigationHeight)
        const sections = []
        collectSections(contentItem, 0, capacity, sections)
        const result = []
        let start = 0
        let end = 0
        for (const section of sections) {
            if (section.end - start > capacity && end > start) {
                result.push({
                                start: start,
                                end: end
                            })
                start = section.start
            }
            // Exceptionally tall text or a single visual still remains accessible
            // in bounded continuation pages, rather than being dropped or scrolled.
            while (section.end - start > capacity) {
                result.push({
                                start: start,
                                end: start + capacity
                            })
                start += capacity
            }
            end = section.end
        }
        if (end > start)
            result.push({
                            start: start,
                            end: end
                        })
        return result.length ? result : [
                                   {
                                       start: 0,
                                       end: 0
                                   }
                               ]
    }
    readonly property int pageCount: pages.length
    readonly property var selectedPage: pages[Math.max(0, Math.min(currentPage, pageCount - 1))]
    readonly property real contentHeight: selectedPage.end - selectedPage.start + (pageCount > 1 ? navigationHeight :
                                                                                                   0)
    implicitHeight: contentHeight

    function collectSections(item, offset, capacity, result) {
        for (const child of item.children) {
            if (!child.visible || child.height <= 0)
                continue
            const start = offset + child.y
            if (child.height > capacity && child.children.length > 0
                    && child instanceof ColumnLayout)
                collectSections(child, start, capacity, result)
            else
                result.push({
                                start: start,
                                end: start + child.height
                            })
        }
    }

    onPageCountChanged: currentPage = Math.min(currentPage, pageCount - 1)

    QQC2.TabBar {
        id: pageTabs
        objectName: "sectionPageTabs"
        width: parent.width
        visible: root.pageCount > 1
        currentIndex: root.currentPage
        onCurrentIndexChanged: {
            if (currentIndex >= 0)
                root.currentPage = currentIndex
        }
        Repeater {
            model: root.pageCount > 1 ? root.pageCount : 0
            QQC2.TabButton {
                required property int index
                text: qsTr("Page %1").arg(index + 1)
                width: Math.max(implicitWidth, pageTabs.width / root.pageCount)
            }
        }
    }

    Item {
        id: viewport
        objectName: "sectionPageViewport"
        y: root.pageCount > 1 ? root.navigationHeight : 0
        width: parent.width
        height: root.selectedPage.end - root.selectedPage.start
        clip: true
    }

    Binding {
        target: root.contentItem
        property: "y"
        value: -root.selectedPage.start
    }

    // Tab traversal must not leave keyboard focus in a clipped, off-page section.
    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() {
            const focused = root.Window.window.activeFocusItem
            let ancestor = focused
            while (ancestor && ancestor !== root.contentItem)
                ancestor = ancestor.parent
            if (!focused || !ancestor)
                return
            const top = focused.mapToItem(root.contentItem, 0, 0).y
            const index = root.pages.findIndex(page => top >= page.start && top < page.end)
            if (index >= 0)
                root.currentPage = index
        }
    }
}
