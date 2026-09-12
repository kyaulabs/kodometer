import QtQuick
import QtTest
import "../../applet" as Applet

TestCase {
    name: "QuotaPresentation"
    when: windowShown
    visible: true
    width: 400
    height: 300

    Component {
        id: presentationComponent
        Applet.QuotaPresentation {}
    }

    Component {
        id: historyComponent
        Applet.QuotaHistoryChart {
            width: 350
        }
    }

    function test_historyRangesAndSelection() {
        const chart = createTemporaryObject(historyComponent, this, {
                                                endTime: 3000000,
                                                windows: [
                                                    {
                                                        kind: "session",
                                                        label: "Session"
                                                    },
                                                    {
                                                        kind: "weekly",
                                                        label: "Weekly"
                                                    }
                                                ],
                                                series: [
                                                    {
                                                        kind: "session",
                                                        points: [[1000000, 80, 0], [2900000, 70, 0],
                                                            [2999900, 65, 0]]
                                                    },
                                                    {
                                                        kind: "weekly",
                                                        points: [[2999900, 30, 0]]
                                                    }
                                                ]
                                            })
        verify(chart)
        compare(chart.rangeDays, 1)
        compare(chart.points.length, 1)
        chart.rangeDays = 7
        compare(chart.points.length, 2)
        chart.rangeDays = 30
        compare(chart.points.length, 3)
        chart.selectedKind = "weekly"
        compare(chart.points[0][1], 30)
        chart.windows = [
                    {
                        kind: "session"
                    }
                ]
        compare(chart.selectedKind, "session")
        compare(chart.valueAt(2000000), 80)
        compare(chart.valueAt(2950000), 70)
        chart.windows = []
        compare(chart.visible, false)
        compare(chart.points.length, 0)
    }

    function test_historyCarriesLastObservationAndHover() {
        const chart = createTemporaryObject(historyComponent, this, {
                                                endTime: 100000,
                                                windows: [
                                                    {
                                                        kind: "session"
                                                    }
                                                ],
                                                series: [
                                                    {
                                                        kind: "session",
                                                        points: [[20000, 80, 0], [50000, 60, 0], [80000,
                                                                                                  100, 90000]]
                                                    }
                                                ]
                                            })
        verify(chart)
        compare(chart.valueAt(19000), null)
        compare(chart.valueAt(20000), 80)
        compare(chart.valueAt(49999), 80)
        compare(chart.valueAt(50000), 60)
        compare(chart.valueAt(79999), 60)
        compare(chart.valueAt(80000), 100)
        compare(chart.valueAt(100000), 100)
        compare(chart.plotPoints, [[20000, 80], [50000, 80], [50000, 60], [80000, 60], [80000, 100],
                                   [100000, 100]])
        const plot = findChild(chart, "quotaHistoryPlot")
        const marker = findChild(chart, "quotaHistoryCursor")
        tryCompare(plot, "available", true)
        plot.requestPaint()
        wait(100)
        waitForRendering(plot)
        const pixels = grabImage(plot)
        // Between grid lanes: no fill before data, continuous fill after it.
        const emptyColor = pixels.pixel(0, 85).toString()
        for (let x = Math.ceil(plot.width * (20000 - 13600) / 86400) + 1; x < Math.floor(
                 plot.width); ++x)
            verify(pixels.pixel(x, 85).toString() !== emptyColor, "Missing chart fill at x=" + x)
        mouseMove(plot, plot.width * (60000 - 13600) / 86400, 40)
        tryCompare(marker, "visible", true)
        compare(chart.hoverValue, 60)
        fuzzyCompare(marker.x, plot.width * (60000 - 13600) / 86400, 1)
        mouseMove(this, 399, 299)
        tryCompare(marker, "visible", false)
        mouseMove(plot, plot.width * (60000 - 13600) / 86400, 40)
        tryCompare(marker, "visible", true)
        verify(findChild(chart, "quotaHistoryTooltip").text.includes("60%"))
        mouseMove(plot, 0, 40)
        tryCompare(marker, "visible", false)
        chart.endTime = 140000
        compare(chart.plotPoints[0], [53600, 60])
        chart.series = [
                    {
                        kind: "session",
                        points: [[20000, 80, 0]]
                    }
                ]
        compare(chart.plotPoints, [[53600, 80], [140000, 80]])
        chart.series = []
        compare(chart.plotPoints.length, 0)
        compare(chart.valueAt(90000), null)
        compare(chart.hoverValue, null)
    }

    function test_providerColorsAreValidatedAndDoNotMutateSnapshots() {
        const source = [
                  {
                      id: "codex",
                      display: {
                          accentColor: "#49a3b0"
                      },
                      accounts: [
                          {
                              windows: []
                          }
                      ],
                      windows: []
                  }
              ]
        const model = createTemporaryObject(presentationComponent, this, {
                                                providers: source
                                            })
        verify(model)
        model.providerColors = '{"codex":"#123456"}'
        compare(model.displayedProviders[0].display.accentColor, "#123456")
        compare(model.displayedProviders[0].accounts[0].display.accentColor, "#123456")
        compare(source[0].display.accentColor, "#49a3b0")
        for (const invalid of ['{}', 'null', '[]', 'broken', '{"codex":"red"}', '{"codex":123}']) {
            model.providerColors = invalid
            compare(model.displayedProviders[0].display.accentColor, "#49a3b0")
        }
    }

    function test_sparkToggleOverridesLegacyPerWindowHiding() {
        const model = createTemporaryObject(presentationComponent, this, {
                                                hiddenWindows: ["codex/model-spark",
                                                    "codex/model-spark-weekly", "codex/weekly"],
                                                providers: [
                                                    {
                                                        id: "codex",
                                                        windows: [
                                                            {
                                                                kind: "session"
                                                            },
                                                            {
                                                                kind: "weekly"
                                                            },
                                                            {
                                                                kind: "model-spark"
                                                            },
                                                            {
                                                                kind: "model-spark-weekly"
                                                            },
                                                            {
                                                                kind: "model-spark-idle",
                                                                idle: true
                                                            }
                                                        ]
                                                    }
                                                ]
                                            })
        compare(model.displayedProviders[0].windows.length, 1)
        model.showSpark = true
        compare(model.displayedProviders[0].windows.length, 3)
        model.showIdleWindows = true
        compare(model.displayedProviders[0].windows.length, 4)
        model.showSpark = false
        compare(model.displayedProviders[0].windows.length, 1)
    }

    function test_filtersWithoutMutatingSnapshots() {
        const source = [
                  {
                      id: "codex",
                      windows: [
                          {
                              kind: "session",
                              remainingPercent: 65
                          },
                          {
                              kind: "model-gpt-5-3-codex-spark",
                              remainingPercent: 100
                          },
                          {
                              kind: "weekly",
                              idle: true,
                              remainingPercent: 100
                          }
                      ]
                  }
              ]
        const model = createTemporaryObject(presentationComponent, this, {
                                                providers: source
                                            })
        compare(model.displayedProviders[0].windows.length, 1)
        compare(source[0].windows.length, 3)
        model.showSpark = true
        compare(model.displayedProviders[0].windows.length, 2)
        model.showIdleWindows = true
        compare(model.displayedProviders[0].windows.length, 3)
        model.hiddenWindows = ["codex/session"]
        compare(model.displayedProviders[0].windows.length, 2)
        compare(model.catalog.length, 3)
    }

    function test_providerRingsExcludeBalanceAndPendingData() {
        const model = createTemporaryObject(presentationComponent, this, {
                                                providers: [
                                                    {
                                                        id: "codex",
                                                        windows: [
                                                            {
                                                                kind: "session",
                                                                remainingPercent: 65
                                                            }
                                                        ]
                                                    },
                                                    {
                                                        id: "kimi",
                                                        windows: [
                                                            {
                                                                kind: "weekly",
                                                                remainingPercent: 30
                                                            },
                                                            {
                                                                kind: "session",
                                                                remainingPercent: 100
                                                            }
                                                        ]
                                                    },
                                                    {
                                                        id: "deepseek",
                                                        cost: {
                                                            balance: 1
                                                        }
                                                    },
                                                    {
                                                        id: "claude",
                                                        pendingSelection: true
                                                    },
                                                    {
                                                        id: "gemini",
                                                        windows: [
                                                            {
                                                                kind: "session",
                                                                remainingPercent: null
                                                            }
                                                        ]
                                                    }
                                                ]
                                            })
        compare(model.quotaProviders.length, 2)
        compare(model.quotaProviders[0].id, "codex")
        compare(model.quotaProviders[1].windows.length, 2)
        model.hiddenWindows = ["kimi/weekly", "kimi/session"]
        compare(model.quotaProviders.length, 1)
    }
}
