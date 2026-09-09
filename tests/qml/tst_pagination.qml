import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtTest
import "../../applet" as Applet

TestCase {
    name: "Pagination"
    when: windowShown
    visible: true
    width: 400
    height: 600

    Component {
        id: pageComponent
        Applet.PaginatedPage {
            id: page
            width: 380
            height: contentHeight
            maximumContentHeight: 250
            contentItem: ColumnLayout {
                parent: page.contentHost
                width: page.width
                spacing: 10
                Rectangle {
                    implicitHeight: 100
                    Layout.fillWidth: true
                }
                Rectangle {
                    implicitHeight: 100
                    Layout.fillWidth: true
                }
                QQC2.Button {
                    objectName: "lastSection"
                    implicitHeight: 100
                    Layout.fillWidth: true
                    text: "Last"
                }
            }
        }
    }

    function test_pagesFitAndRetainWholeSections() {
        const page = createTemporaryObject(pageComponent, this)
        verify(page)
        tryCompare(page, "pageCount", 2)
        verify(page.contentHeight <= 250)
        compare(page.currentPage, 0)
        compare(page.pages[0].end, 210)
        const tabs = findChild(page, "sectionPageTabs")
        verify(tabs.visible)
        tabs.currentIndex = 1
        tryCompare(page, "currentPage", 1)
        verify(page.contentHeight <= 250)
        compare(page.pages[1].start, 220)
        const last = findChild(page, "lastSection")
        tabs.forceActiveFocus()
        page.currentPage = 0
        last.forceActiveFocus()
        verify(last.activeFocus)
        tryCompare(page, "currentPage", 1)
        page.maximumContentHeight = 500
        tryCompare(page, "pageCount", 1)
        tryCompare(page, "currentPage", 0)
        compare(page.contentHeight, 320)
        page.maximumContentHeight = 160
        tryCompare(page, "pageCount", 3)
        for (let index = 0; index < page.pageCount; ++index) {
            page.currentPage = index
            verify(page.contentHeight <= 160)
        }
        last.visible = false
        tryCompare(page, "pageCount", 2)
        verify(page.currentPage < page.pageCount)
    }
}
