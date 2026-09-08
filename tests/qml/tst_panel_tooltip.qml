import QtQuick
import QtTest
import "../../applet" as Applet

TestCase {
    name: "PanelToolTip"
    Component {
        id: summaryComponent
        Applet.PanelToolTip {}
    }

    function window(kind, remaining, idle = false) {
        return {
            kind: kind,
            remainingPercent: remaining,
            idle: idle
        }
    }

    function test_barSummaryAndDonutScopes() {
        const summary = createTemporaryObject(summaryComponent, this, {
                                                  providers: [
                                                      {
                                                          id: "codex",
                                                          windows: [window("session", 90), window(
                                                                  "weekly", 75)]
                                                      },
                                                      {
                                                          id: "kimi",
                                                          windows: [window("weekly", 80), window(
                                                                  "session", 60)]
                                                      },
                                                      {
                                                          id: "deepseek",
                                                          cost: {
                                                              balance: 1,
                                                              currencyCode: "USD"
                                                          }
                                                      },
                                                      {
                                                          id: "openrouter",
                                                          cost: {
                                                              balance: 2,
                                                              currencyCode: "USD"
                                                          },
                                                          windows: [window("spend", 5)]
                                                      }
                                                  ]
                                              })
        compare(summary.mainText, "Kodometer")
        compare(summary.subText,
                "Codex: 75% weekly remaining\nKimi Code: 80% weekly remaining\nDeepSeek Balance: $1.00\nOpenRouter Balance: $2.00")
        summary.donutCharts = true
        compare(summary.mainText, "")
        compare(summary.subText, "")
        summary.hoveredQuota = "session"
        compare(summary.mainText, "Kodometer — Session quota")
        compare(summary.subText, "Codex: 90% session remaining\nKimi Code: 60% session remaining")
        summary.hoveredQuota = "weekly"
        compare(summary.mainText, "Kodometer — Weekly quota")
        compare(summary.subText, "Codex: 75% weekly remaining\nKimi Code: 80% weekly remaining")
        summary.providers = []
        compare(summary.subText, "No weekly quota reported")
        summary.hoveredQuota = "session"
        compare(summary.subText, "No session quota reported")
        summary.donutCharts = false
        compare(summary.subText, "No provider data")
    }

    function test_codingQuotaRemainsAuthoritativeOverOptionalBalance() {
        const summary = createTemporaryObject(summaryComponent, this, {
                                                  providers: [
                                                      {
                                                          id: "zai",
                                                          cost: {
                                                              balance: 99,
                                                              currencyCode: "CNY"
                                                          },
                                                          windows: [window("weekly", 40), window(
                                                                  "session", 80)]
                                                      }
                                                  ]
                                              })
        compare(summary.subText, "z.ai: 40% weekly remaining")
    }

    function test_malformedMissingAndPrivateData() {
        const summary = createTemporaryObject(summaryComponent, this, {
                                                  providers: [
                                                      {
                                                          id: "codex",
                                                          name: "<b>private</b>",
                                                          accountName: "private",
                                                          windows: [window("weekly", null), window(
                                                                  "session", 12.55)]
                                                      },
                                                      {
                                                          id: "claude",
                                                          windows: [window("weekly", NaN), window(
                                                                  "weekly", Infinity), window(
                                                                  "weekly", "75"), window("weekly",
                                                                                          true)]
                                                      },
                                                      {
                                                          id: "gemini",
                                                          windows: [window("model-pro-daily", 70),
                                                              window("model-flash-daily", 30)]
                                                      },
                                                      {
                                                          id: "kimi",
                                                          windows: [window("weekly", -5), window(
                                                                  "weekly", 200), window("weekly", 0,
                                                                                         true)]
                                                      },
                                                      {
                                                          id: "deepseek",
                                                          cost: {
                                                              balance: 2.5,
                                                              currencyCode: "CNY"
                                                          }
                                                      },
                                                      {
                                                          id: "xai",
                                                          cost: {
                                                              balanceUSD: 0
                                                          }
                                                      },
                                                      {
                                                          id: "zai",
                                                          pending: true,
                                                          cost: {
                                                              balance: 100
                                                          }
                                                      },
                                                      {
                                                          id: "unknown",
                                                          name: "private",
                                                          windows: [window("weekly", 10)]
                                                      }
                                                  ]
                                              })
        compare(summary.subText,
                "Codex: 12.6% session remaining\nClaude: Not reported\nGemini: 30% quota remaining\nKimi Code: 0% weekly remaining\nDeepSeek Balance: 2.50 CNY\nxAI Balance: $0.00")
        summary.providers = [
                    {
                        id: "deepseek",
                        cost: {
                            balance: null
                        }
                    },
                    {
                        id: "xai",
                        cost: {
                            balanceUSD: Infinity
                        }
                    }
                ]
        compare(summary.subText, "DeepSeek: Not reported\nxAI: Not reported")
        summary.providers = [
                    {
                        id: "deepseek",
                        cost: {
                            balance: -1.25,
                            currencyCode: "<private>"
                        }
                    }
                ]
        compare(summary.subText, "DeepSeek: Not reported")
        summary.providers = [
                    {
                        id: "deepseek",
                        cost: {
                            balanceUSD: -1.25
                        }
                    }
                ]
        compare(summary.subText, "DeepSeek Balance: $-1.25")
        summary.providers = [
                    {
                        id: "codex",
                        windows: [window("weekly", 25), window("weekly", 15)]
                    }
                ]
        compare(summary.subText, "Codex: 15% weekly remaining")
        summary.providers = []
        // Context invalidation must immediately remove the old values.
        compare(summary.subText, "No provider data")
    }
}
