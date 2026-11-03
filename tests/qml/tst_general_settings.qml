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
        compare(scrollBar.policy, ScrollBar.AlwaysOn)
        verify(scrollBar.size < 1)
        tryCompare(scrollBar, "opacity", 1)
        tryCompare(scrollBar, "visible", true)
        verify(scrollBar.width > 0)
        const barPosition = scrollBar.mapToItem(page, 0, 0)
        verify(barPosition.x >= 0 && barPosition.x + scrollBar.width <= page.width)
        verify(barPosition.y >= 0 && barPosition.y + scrollBar.height <= page.height)
        verify(flick.contentY <= flick.originY + 1, "Page must start at the top")
        const first = findChild(page, "refreshInterval")
        verify(first.mapToItem(page, 0, 0).y >= 0)
        mouseDrag(scrollBar, scrollBar.width / 2, scrollBar.height * scrollBar.size / 2, 0,
                  scrollBar.height * (1 - scrollBar.size))
        tryVerify(() => flick.contentY > flick.originY)
        flick.contentY = flick.contentHeight - flick.height + flick.originY
        const last = findChild(page, "provider-openrouter")
        tryVerify(() => last.mapToItem(page, 0, 0).y < page.height)
        last.click()
        verify(page.cfg_disabledProviders.includes("openrouter"))
        page.height = flick.contentHeight + 100
        tryVerify(() => flick.contentY <= flick.originY + 1)
        tryCompare(scrollBar, "policy", ScrollBar.AlwaysOff)
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

    function test_stagesQuotaWindowAndBarPreferences() {
        const page = createTemporaryObject(settingsComponent, this)
        compare(page.cfg_showOpenAISpark, false)
        compare(page.cfg_quotaBarsRemaining, true)
        compare(page.cfg_hiddenQuotaWindows.length, 0)
        findChild(page, "showOpenAISpark").click()
        compare(page.cfg_showOpenAISpark, true)
        findChild(page, "quotaBarsRemaining").click()
        compare(page.cfg_quotaBarsRemaining, false)
        page.cfg_quotaWindowCatalog = JSON.stringify([
                                                         {
                                                             key: "openai/session",
                                                             provider: "OpenAI",
                                                             label: "<b>Session</b>"
                                                         }
                                                     ])
        const window = findChild(page, "quota-window-openai/session")
        verify(window)
        compare(window.contentItem.textFormat, Text.PlainText)
        compare(window.contentItem.text, "OpenAI — <b>Session</b>")
        window.click()
        compare(page.cfg_hiddenQuotaWindows[0], "openai/session")
        window.click()
        compare(page.cfg_hiddenQuotaWindows.length, 0)
        page.cfg_hiddenQuotaWindows = ["openai/session"]
        compare(window.checked, false)
        page.cfg_quotaWindowCatalog = "broken"
        compare(page.quotaWindows.length, 0)
        page.cfg_quotaWindowCatalog = "{}"
        compare(page.quotaWindows.length, 0)
        page.cfg_quotaWindowCatalog = '[{},null,{"key":1}]'
        compare(page.quotaWindows.length, 0)
    }

    function test_onlyProviderColorsRemain() {
        const page = createTemporaryObject(settingsComponent, this)
        compare(findChild(page, "panelSessionColor"), null)
        compare(findChild(page, "panelWeeklyColor"), null)
        compare(findChild(page, "provider-color-openai").fallbackColor, "#a28be0")
        compare(findChild(page, "provider-color-kimi").fallbackColor, "#49a3b0")
        compare(findChild(page, "provider-color-openrouter").fallbackColor, "#c8ff00")
    }

    function test_sparkHasOneControl() {
        const page = createTemporaryObject(settingsComponent, this, {
                                               cfg_quotaWindowCatalog: JSON.stringify([
                                                                                          {
                                                                                              key: "openai/session",
                                                                                              provider: "OpenAI",
                                                                                              label: "Session"
                                                                                          },
                                                                                          {
                                                                                              key: "openai/model-gpt-5-3-codex-spark",
                                                                                              provider: "OpenAI",
                                                                                              label: "Spark"
                                                                                          },
                                                                                          {
                                                                                              key: "openai/model-gpt-5-3-codex-spark-weekly",
                                                                                              provider: "OpenAI",
                                                                                              label: "Spark Weekly"
                                                                                          }
                                                                                      ])
                                           })
        compare(page.quotaWindows.length, 1)
        verify(findChild(page, "showOpenAISpark"))
        verify(findChild(page, "quota-window-openai/session"))
        compare(findChild(page, "quota-window-openai/model-gpt-5-3-codex-spark"), null)
        compare(findChild(page, "quota-window-openai/model-gpt-5-3-codex-spark-weekly"), null)
        findChild(page, "showOpenAISpark").click()
        compare(page.cfg_showOpenAISpark, true)
    }

    function test_providerColorsFollowApplyCancelAndDefaults() {
        const page = createTemporaryObject(settingsComponent, this)
        const openai = findChild(page, "provider-color-openai")
        const kimi = findChild(page, "provider-color-kimi")
        verify(openai)
        verify(kimi)
        compare(page.cfg_providerColors, "{}")
        openai.colorValue = "#123456"
        compare(JSON.parse(page.cfg_providerColors).openai, "#123456")
        kimi.colorValue = "#abcdef"
        compare(JSON.parse(page.cfg_providerColors).openai, "#123456")
        compare(JSON.parse(page.cfg_providerColors).kimi, "#abcdef")
        page.cfg_providerColors = '{"openai":"#654321"}'
        compare(openai.colorValue, "#654321")
        compare(kimi.colorValue, "")
        page.cfg_providerColors = "{}"
        compare(openai.colorValue, "")
        openai.colorValue = "#112233"
        findChild(openai, "resetColor").click()
        compare(page.cfg_providerColors, "{}")
    }

    function test_providerSwitchesAndReset() {
        const page = createTemporaryObject(settingsComponent, this)
        const openai = findChild(page, "provider-openai")
        const claude = findChild(page, "provider-claude")
        verify(openai)
        verify(claude)
        compare(openai.checked, true)
        openai.click()
        compare(page.cfg_disabledProviders, ["openai"])
        claude.click()
        compare(page.cfg_disabledProviders, ["openai", "claude"])
        openai.click()
        compare(page.cfg_disabledProviders, ["claude"])
        page.cfg_disabledProviders = []
        compare(openai.checked, true)
        compare(claude.checked, true)
        page.cfg_disabledProviders = ["openai", "unknown-provider"]
        compare(openai.checked, false)
        compare(claude.checked, true)
        claude.click()
        compare(page.cfg_disabledProviders, ["openai", "unknown-provider", "claude"])
    }
}
