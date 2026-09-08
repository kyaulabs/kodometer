import QtQuick

QtObject {
    id: root

    property var providers: []
    property bool donutCharts: false
    property string hoveredQuota
    readonly property string mainText: !donutCharts ? qsTr("Kodometer") : (hoveredQuota
                                                                           === "session" ? qsTr(
                                                                                               "Kodometer — Session quota") :
                                                                                           (hoveredQuota
                                                                                            === "weekly"
                                                                                            ? qsTr("Kodometer — Weekly quota") :
                                                                                              ""))
    readonly property string subText: donutCharts && hoveredQuota === "" ? "" : summary(donutCharts
                                                                                        ? hoveredQuota :
                                                                                          "")

    function validNumber(value) {
        return typeof value === "number" && Number.isFinite(value)
    }

    function summary(kind) {
        // Only public names and normalized numeric usage enter the panel tooltip.
        const names = {
            codex: "Codex",
            claude: "Claude",
            gemini: "Gemini",
            kimi: "Kimi Code",
            deepseek: "DeepSeek",
            openrouter: "OpenRouter",
            xai: "xAI",
            zai: "z.ai"
        }
        const lines = []
        for (const provider of providers) {
            const name = names[provider.id]
            if (typeof name !== "string" || provider.pending)
                continue
            const cost = provider.cost || {}
            const balance = cost.balance !== undefined ? cost.balance : cost.balanceUSD
            const currency = cost.currencyCode === undefined ? "USD" : cost.currencyCode
            const balanceProvider = ["deepseek", "xai", "openrouter"].includes(provider.id)
            if (kind === "" && balanceProvider && validNumber(balance) && typeof currency
                    === "string" && /^[A-Z]{3}$/.test(currency)) {
                const money = currency === "USD" ? "$" + balance.toFixed(2) : balance.toFixed(2) + " "
                                                   + currency
                lines.push(qsTr("%1 Balance: %2").arg(name).arg(money))
                continue
            }
            const windows = (provider.windows || []).filter(window => !window.idle && validNumber(
                                                                          window.remainingPercent)
                                                                      && (kind === ""
                                                                          || window.kind === kind))
            let selected = windows
            if (kind === "") {
                const weekly = windows.filter(window => window.kind === "weekly")
                const session = windows.filter(window => window.kind === "session")
                selected = weekly.length ? weekly : (session.length ? session : windows)
            }
            if (selected.length) {
                const remaining = Math.max(0, Math.min(100, Math.min(...selected.map(window
                                                                                     => window.remainingPercent))))
                const percent = Number(remaining.toFixed(1)).toString()
                const selectedKind = selected[0].kind
                const text = selectedKind === "weekly" ? qsTr("%1: %2% weekly remaining") : (
                                                             selectedKind === "session" ? qsTr(
                                                                                              "%1: %2% session remaining") :
                                                                                          qsTr("%1: %2% quota remaining"))
                lines.push(text.arg(name).arg(percent))
            } else if (kind === "") {
                lines.push(qsTr("%1: Not reported").arg(name))
            }
        }
        if (lines.length)
            return lines.join("\n")
        return kind === "weekly" ? qsTr("No weekly quota reported") : (kind === "session" ? qsTr(
                                                                                                "No session quota reported") :
                                                                                            qsTr("No provider data"))
    }
}
