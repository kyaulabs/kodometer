.pragma library

function migrate(configuration) {
    if (configuration.openaiMigrationComplete)
        return
    configuration.disabledProviders = configuration.disabledProviders.map(
                id => id === "codex" ? "openai" : id)
    configuration.hiddenQuotaWindows = configuration.hiddenQuotaWindows.map(
                id => id.replace(/^codex\//, "openai/"))
    configuration.showOpenAISpark = configuration.showOpenAISpark || configuration.showCodexSpark
    try {
        const colors = JSON.parse(configuration.providerColors)
        if (colors.codex !== undefined && colors.openai === undefined)
            colors.openai = colors.codex
        delete colors.codex
        configuration.providerColors = JSON.stringify(colors)
    } catch (_) {} // Leave malformed preferences untouched for normal validation.
    try {
        const catalog = JSON.parse(configuration.quotaWindowCatalog)
        for (const entry of catalog) {
            if (typeof entry.key === "string" && entry.key.startsWith("codex/")) {
                entry.key = entry.key.replace(/^codex\//, "openai/")
                entry.provider = "OpenAI"
            }
        }
        configuration.quotaWindowCatalog = JSON.stringify(catalog)
    } catch (_) {}
    // OAuthProfiles validates and normalizes old profile keys at its storage boundary.
    configuration.openaiMigrationComplete = true
    configuration.writeConfig()
}
