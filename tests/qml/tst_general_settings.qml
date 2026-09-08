import QtQuick
import QtQuick.Controls
import QtTest
import "../../applet/config" as Config

TestCase {
    name: "GeneralSettings"
    when: windowShown
    visible: true
    width: 640
    height: 640

    Component {
        id: settingsComponent
        Config.ConfigGeneral {
            width: 600
        }
    }

    function test_scrollsFromTopToLastProvider() {
        const page = createTemporaryObject(settingsComponent, this, {
                                               width: 420,
                                               height: 280
                                           })
        verify(page.flickable, "General must own a scrolling viewport")
        const flick = page.flickable
        tryVerify(() => flick.contentHeight > flick.height)
        const scrollBar = page.contentItem.ScrollBar.vertical
        verify(scrollBar)
        compare(scrollBar.policy, ScrollBar.AsNeeded)
        verify(scrollBar.size < 1)
        verify(flick.contentY <= flick.originY + 1, "Page must start at the top")
        const first = findChild(page, "refreshInterval")
        verify(first.mapToItem(page, 0, 0).y >= 0)
        flick.contentY = flick.contentHeight - flick.height + flick.originY
        const last = findChild(page, "provider-openrouter")
        tryVerify(() => last.mapToItem(page, 0, 0).y < page.height)
        last.click()
        verify(page.cfg_disabledProviders.includes("openrouter"))
        page.height = 1200
        tryVerify(() => flick.contentY <= flick.originY + 1)
    }

    function test_defaultsAndBindings() {
        const page = createTemporaryObject(settingsComponent, this)
        verify(page)
        compare(page.cfg_autoRefresh, true)
        compare(page.cfg_refreshIntervalMinutes, 5)
        compare(page.cfg_showIdleWindows, false)
        compare(page.cfg_panelDonutCharts, false)
        compare(page.cfg_panelSystemAccent, false)
        page.cfg_panelDonutCharts = true
        compare(findChild(page, "panelDonutCharts").currentIndex, 1)
        page.cfg_panelSystemAccent = true
        compare(findChild(page, "panelSystemAccent").currentIndex, 1)
        page.cfg_panelDonutCharts = false
        page.cfg_panelSystemAccent = false
        compare(findChild(page, "panelDonutCharts").currentIndex, 0)
        compare(findChild(page, "panelSystemAccent").currentIndex, 0)
        compare(page.cfg_quotaNotifications, false)
        compare(page.cfg_quotaNotificationThreshold, 10)
        const threshold = findChild(page, "quotaNotificationThreshold")
        verify(threshold)
        compare(threshold.enabled, false)
        page.cfg_quotaNotifications = true
        compare(threshold.enabled, true)
        page.cfg_quotaNotificationThreshold = 20
        compare(threshold.value, 20)
        compare(page.cfg_disabledProviders.length, 0)
        const interval = findChild(page, "refreshInterval")
        verify(interval)
        page.cfg_autoRefresh = false
        compare(interval.enabled, false)
        page.cfg_autoRefresh = true
        compare(interval.enabled, true)
        page.cfg_refreshIntervalMinutes = 10
        compare(interval.value, 10)
        page.cfg_showIdleWindows = true
        compare(findChild(page, "showIdleWindows").checked, true)
    }

    function test_providerSwitchesAndReset() {
        const page = createTemporaryObject(settingsComponent, this)
        const codex = findChild(page, "provider-codex")
        const claude = findChild(page, "provider-claude")
        verify(codex)
        verify(claude)
        compare(codex.checked, true)
        codex.click()
        compare(page.cfg_disabledProviders, ["codex"])
        claude.click()
        compare(page.cfg_disabledProviders, ["codex", "claude"])
        codex.click()
        compare(page.cfg_disabledProviders, ["claude"])
        page.cfg_disabledProviders = []
        compare(codex.checked, true)
        compare(claude.checked, true)
        page.cfg_disabledProviders = ["codex", "unknown-provider"]
        compare(codex.checked, false)
        compare(claude.checked, true)
        claude.click()
        compare(page.cfg_disabledProviders, ["codex", "unknown-provider", "claude"])
    }
}
