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
        meter.providers = [
                    {
                        id: "codex",
                        windows: [
                            {
                                kind: "session",
                                remainingPercent: 12.8
                            }
                        ]
                    },
                    {
                        id: "kimi",
                        windows: [
                            {
                                kind: "weekly",
                                remainingPercent: 55.1
                            }
                        ]
                    }
                ]
        const session = findChild(meter, "provider-meter-codex")
        const weekly = findChild(meter, "provider-meter-kimi")
        verify(session)
        verify(weekly)
        compare(session.visible, true)
        compare(session.windows[0].remainingPercent, 12.8)
        compare(weekly.windows[0].remainingPercent, 55.1)
        verify(session.quotaDescription.includes("12.8% remaining"))
        compare(session.width, session.height)
        verify(weekly.x > session.x)
        meter.vertical = true
        meter.width = 56
        meter.height = 120
        verify(weekly.y > session.y)
        compare(session.width, session.height)
        meter.sessionRemaining = -10
        compare(meter.sessionFillWidth, 0)
        meter.sessionRemaining = 120
        compare(meter.sessionFillWidth, meter.width)
        meter.sessionRemaining = NaN
        compare(meter.sessionFillWidth, 0)
        meter.weeklyRemaining = Infinity
        compare(meter.weeklyFillWidth, 0)
        meter.donutCharts = false
        compare(findChild(meter, "provider-meter-codex"), null)
    }

    function test_donutColorsFollowProvider() {
        const meter = createTemporaryObject(compactMeterComponent, this, {
                                                donutCharts: true,
                                                providers: [
                                                    {
                                                        id: "codex",
                                                        display: {
                                                            accentColor: "#778899"
                                                        },
                                                        windows: [
                                                            {
                                                                kind: "session",
                                                                remainingPercent: 65
                                                            },
                                                            {
                                                                kind: "weekly",
                                                                remainingPercent: 20
                                                            }
                                                        ]
                                                    }
                                                ]
                                            })
        const provider = findChild(meter, "provider-meter-codex")
        const session = findChild(meter, "quota-ring-codex-session")
        const weekly = findChild(meter, "quota-ring-codex-weekly")
        compare(session.accentColor, provider.accentColor)
        verify(weekly.accentColor.toString() !== session.accentColor.toString())
        const windows = meter.providers[0].windows
        meter.providers = [
                    {
                        id: "codex",
                        display: {
                            accentColor: "#112233"
                        },
                        windows: windows
                    }
                ]
        const updatedSession = findChild(meter, "quota-ring-codex-session")
        const updatedWeekly = findChild(meter, "quota-ring-codex-weekly")
        compare(updatedSession.accentColor, "#112233")
        verify(updatedWeekly.accentColor.toString() !== updatedSession.accentColor.toString())
        compare(meter.barColor("session"), "#112233")
        compare(meter.barColor("weekly"), "#112233")
    }

    function test_donutToolTipsHaveSeparateHitAreas() {
        const meter = createTemporaryObject(compactMeterComponent, this, {
                                                x: 100,
                                                y: 100,
                                                width: 120,
                                                height: 56,
                                                donutCharts: true,
                                                providers: [
                                                    {
                                                        id: "codex",
                                                        windows: [
                                                            {
                                                                kind: "session",
                                                                remainingPercent: 90
                                                            },
                                                            {
                                                                kind: "weekly",
                                                                remainingPercent: 75
                                                            }
                                                        ]
                                                    },
                                                    {
                                                        id: "kimi",
                                                        name: "Kimi Code",
                                                        windows: [
                                                            {
                                                                kind: "weekly",
                                                                remainingPercent: 30
                                                            }
                                                        ]
                                                    }
                                                ]
                                            })
        // The click target overlays the meter in the applet, but must not eat hover events.
        const overlay = Qt.createQmlObject('import QtQuick; MouseArea { anchors.fill: parent }',
                                           meter)
        const session = findChild(meter, "provider-tooltip-codex")
        const weekly = findChild(meter, "provider-tooltip-kimi")
        verify(session.subText.includes("90.0% remaining"))
        verify(session.subText.includes("75.0% remaining"))
        verify(!session.subText.includes("30.0%"))
        compare(weekly.mainText, "Kodometer — Kimi Code")
        verify(weekly.subText.includes("30.0% remaining"))
        compare(session.textFormat, Text.PlainText)
        compare(weekly.textFormat, Text.PlainText)
        mouseMove(session, session.width / 2, session.height / 2)
        tryCompare(session, "containsMouse", true)
        compare(weekly.containsMouse, false)
        mouseMove(weekly, weekly.width / 2, weekly.height / 2)
        tryCompare(weekly, "containsMouse", true)
        compare(session.containsMouse, false)
        meter.vertical = true
        meter.width = 56
        meter.height = 120
        mouseMove(session, session.width / 2, session.height / 2)
        tryCompare(session, "containsMouse", true)
        compare(weekly.containsMouse, false)
        meter.toolTipsEnabled = false
        compare(session.active, false)
        compare(weekly.active, false)
        meter.toolTipsEnabled = true
        meter.donutCharts = false
        compare(findChild(meter, "provider-tooltip-codex"), null)
        compare(findChild(meter, "provider-tooltip-kimi"), null)
        mouseMove(this, 1, 1)
        overlay.destroy()
    }

    function test_emptyMeterAndManyNestedWindows() {
        const meter = createTemporaryObject(compactMeterComponent, this, {
                                                donutCharts: true,
                                                width: 24,
                                                height: 24
                                            })
        compare(findChild(meter, "emptyQuotaMeter").visible, true)
        const windows = []
        for (let i = 0; i < 20; ++i)
            windows.push({
                             kind: "model-" + i,
                             remainingPercent: 50
                         })
        meter.providers = [
                    {
                        id: "codex",
                        windows: windows
                    }
                ]
        compare(meter.providerCount, 1)
        compare(findChild(meter, "emptyQuotaMeter").visible, false)
        const inner = findChild(meter, "quota-ring-codex-model-19")
        verify(inner)
        verify(inner.width > 0)
        verify(inner.strokeWidth > 0)
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
