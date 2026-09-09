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

    function test_historyRangesSelectionAndResetGaps() {
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
        verify(chart.connects([100, 65, 1000], [400, 60, 1000]))
        verify(!chart.connects(null, [400, 60, 1000]))
        verify(!chart.connects([100, 65, 1000], [400, 100, 1000]))
        verify(!chart.connects([100, 65, 1000], [400, 60, 2000]))
        verify(!chart.connects([100, 65, 1000], [2000, 60, 1000]))
        chart.windows = []
        compare(chart.visible, false)
        compare(chart.points.length, 0)
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
