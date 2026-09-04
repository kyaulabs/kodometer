import QtQuick
import QtTest
import "../../applet" as Applet

TestCase {
    name: "VisualPrimitives"
    when: windowShown

    Component {
        id: usageBarComponent
        Applet.UsageBar {
            width: 200
            height: 8
        }
    }

    Component {
        id: compactMeterComponent
        Applet.CompactMeter {
            width: 20
            height: 20
        }
    }

    Component {
        id: providerTabsComponent
        Applet.ProviderTabs {
            width: 300
        }
    }

    function test_usageBarClampsPercent() {
        const bar = createTemporaryObject(usageBarComponent, this)
        verify(bar)

        bar.percent = 25
        compare(bar.fillWidth, 50)
        bar.percent = -10
        compare(bar.fillWidth, 0)
        bar.percent = 120
        compare(bar.fillWidth, 200)
    }

    function test_compactMeterShowsRemainingQuota() {
        const meter = createTemporaryObject(compactMeterComponent, this)
        verify(meter)

        meter.sessionRemaining = 75
        meter.weeklyRemaining = 40
        compare(meter.sessionFillWidth, 15)
        compare(meter.weeklyFillWidth, 8)
    }

    function test_providerTabsExposeOverviewAndProviders() {
        const tabs = createTemporaryObject(providerTabsComponent, this, {
                                               providers: [
                                                   {
                                                       id: "codex",
                                                       name: "Codex"
                                                   },
                                                   {
                                                       id: "claude",
                                                       name: "Claude"
                                                   }
                                               ]
                                           })
        verify(tabs)
        compare(tabs.tabCount, 3)

        const overviewSpy = signalSpy.createObject(this, {
                                                       target: tabs,
                                                       signalName: "overviewSelected"
                                                   })
        const providerSpy = signalSpy.createObject(this, {
                                                       target: tabs,
                                                       signalName: "providerSelected"
                                                   })

        tabs.activateTab(0)
        compare(overviewSpy.count, 1)
        tabs.activateTab(2)
        compare(providerSpy.count, 1)
        compare(providerSpy.signalArguments[0][0], "claude")
    }

    function test_providerTabsCanHideOverview() {
        const tabs = createTemporaryObject(providerTabsComponent, this, {
                                               providers: [
                                                   {
                                                       id: "codex",
                                                       name: "Codex"
                                                   }
                                               ],
                                               overviewVisible: false
                                           })
        verify(tabs)
        compare(tabs.tabCount, 1)

        const providerSpy = signalSpy.createObject(this, {
                                                       target: tabs,
                                                       signalName: "providerSelected"
                                                   })
        tabs.activateTab(0)
        compare(providerSpy.signalArguments[0][0], "codex")
        tabs.activateTab(-1)
        tabs.activateTab(4)
        compare(providerSpy.count, 1)
    }
}
