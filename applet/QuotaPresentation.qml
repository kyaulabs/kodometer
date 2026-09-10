import QtQuick

QtObject {
    id: root
    property var providers: []
    property var hiddenWindows: []
    property string providerColors: "{}"
    readonly property var colors: {
        try {
            const value = JSON.parse(providerColors)
            return value && typeof value === "object" && !Array.isArray(value) ? value : ({})
        } catch (error) {
            return ({})
        }
    }
    property bool showSpark: false
    property bool showIdleWindows: false

    function key(providerId, windowData) {
        return providerId + "/" + String(windowData.kind || "")
    }

    function enabled(providerId, windowData) {
        const spark = providerId === "codex" && /spark/i.test(String(windowData.kind || ""))
        // The master Spark switch supersedes obsolete per-window Spark entries.
        return (spark ? showSpark : !hiddenWindows.includes(key(providerId, windowData))) && (showIdleWindows ||
                                                                                              !windowData.idle)
    }

    function filtered(provider) {
        const result = Object.assign({}, provider)
        const custom = colors[provider.id]
        if (typeof custom === "string" && /^#[0-9a-fA-F]{6}$/.test(custom))
            result.display = Object.assign({}, provider.display || {}, {
                                               accentColor: custom
                                           })
        result.windows = (provider.windows || []).filter(windowData => enabled(provider.id,
                                                                               windowData))
        result.accounts = (provider.accounts || []).map(account => {
            const copy = Object.assign({}, account)
            if (result.display)
                copy.display = Object.assign({}, account.display || {}, result.display)
            copy.windows = (account.windows || []).filter(windowData => enabled(provider.id,
                                                                                windowData))
            return copy
        })
        return result
    }

    readonly property var displayedProviders: providers.map(provider => filtered(provider))
    readonly property var quotaProviders: displayedProviders.filter(provider =>
    !provider.pendingSelection).map(provider => {
        const result = Object.assign({}, provider)
        result.windows = provider.windows.filter(windowData => typeof windowData.remainingPercent
                                                               === "number" && Number.isFinite(
                                                                   windowData.remainingPercent)
                                                               && windowData.remainingPercent >= 0
                                                               && windowData.remainingPercent
                                                               <= 100)
        return result
    }).filter(provider => provider.windows.length > 0)
    readonly property var catalog: {
        const entries = []
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
        for (const provider of providers) {
            if (!Object.prototype.hasOwnProperty.call(names, provider.id))
                continue
            for (const windowData of (provider.windows || [])) {
                if (!/^[a-z0-9][a-z0-9-]{0,127}$/.test(String(windowData.kind || "")))
                    continue
                if (entries.length === 128)
                    return entries
                entries.push({
                                 key: key(provider.id, windowData),
                                 provider: names[provider.id],
                                 label: String(windowData.label || windowData.kind).slice(0, 128)
                             })
            }
        }
        return entries
    }
}
