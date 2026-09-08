import QtQuick
import QtTest
import "../../applet" as Applet

TestCase {
    name: "VisualPrimitives"
    when: windowShown
    visible: true
    width: 640
    height: 480

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

    Component {
        id: signalSpyComponent
        SignalSpy {}
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

    function test_compactDonutsAndInvalidQuota() {
        const meter = createTemporaryObject(compactMeterComponent, this, {
                                                width: 120,
                                                height: 56
                                            })
        compare(meter.donutCharts, false)
        meter.donutCharts = true
        meter.sessionRemaining = 12.8
        meter.weeklyRemaining = 55.1
        const session = findChild(meter, "sessionRing")
        const weekly = findChild(meter, "weeklyRing")
        verify(session)
        verify(weekly)
        compare(session.visible, true)
        compare(session.value, 12.8)
        compare(weekly.value, 55.1)
        compare(session.valueText, "12.8%")
        compare(session.width, session.height)
        verify(weekly.x > session.x)
        meter.vertical = true
        meter.width = 56
        meter.height = 120
        verify(weekly.y > session.y)
        compare(session.width, session.height)
        meter.sessionRemaining = -10
        compare(session.value, 0)
        meter.sessionRemaining = 120
        compare(session.value, 100)
        meter.sessionRemaining = NaN
        compare(session.valueText, "—")
        compare(meter.sessionFillWidth, 0)
        meter.weeklyRemaining = Infinity
        compare(weekly.valueText, "—")
        meter.donutCharts = false
        compare(session.visible, false)
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

        const overviewSpy = signalSpyComponent.createObject(this, {
                                                                target: tabs,
                                                                signalName: "overviewSelected"
                                                            })
        const providerSpy = signalSpyComponent.createObject(this, {
                                                                target: tabs,
                                                                signalName: "providerSelected"
                                                            })

        tabs.activateTab(0)
        compare(overviewSpy.count, 1)
        tabs.activateTab(2)
        compare(providerSpy.count, 1)
        compare(providerSpy.signalArguments[0][0], "claude")
    }

    function test_providerTabsCenterTheirContents() {
        const tabs = createTemporaryObject(providerTabsComponent, this, {
                                               providers: [
                                                   {
                                                       id: "codex",
                                                       name: "Codex"
                                                   }
                                               ]
                                           })
        verify(tabs)
        const button = findChild(tabs, "provider-tab-0")
        const content = findChild(tabs, "provider-tab-content-0")
        verify(button)
        verify(content)
        const position = content.mapToItem(button, 0, 0)
        fuzzyCompare(position.y + content.implicitHeight / 2, button.height / 2, 1)
        fuzzyCompare(position.x + content.width / 2, button.width / 2, 1)
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

        const providerSpy = signalSpyComponent.createObject(this, {
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
