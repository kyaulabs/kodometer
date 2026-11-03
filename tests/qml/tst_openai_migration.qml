import QtQuick
import QtTest
import "../../applet/OpenAIMigration.js" as Migration

TestCase {
    name: "OpenAIMigration"

    function test_migratesPreferencesOnce() {
        let writes = 0
        const config = {
            openaiMigrationComplete: false,
            disabledProviders: ["codex", "claude"],
            hiddenQuotaWindows: ["codex/session", "claude/weekly"],
            providerColors: '{"codex":"#112233","claude":"#445566"}',
            quotaWindowCatalog: '[{"key":"codex/session","provider":"Codex","label":"Session"}]',
            showCodexSpark: true,
            showOpenAISpark: false,
            writeConfig: () => writes++
        }
        Migration.migrate(config)
        compare(config.disabledProviders, ["openai", "claude"])
        compare(config.hiddenQuotaWindows, ["openai/session", "claude/weekly"])
        compare(JSON.parse(config.providerColors).openai, "#112233")
        compare(JSON.parse(config.providerColors).claude, "#445566")
        verify(JSON.parse(config.providerColors).codex === undefined)
        compare(JSON.parse(config.quotaWindowCatalog)[0].key, "openai/session")
        verify(config.showOpenAISpark)
        compare(writes, 1)
        config.showOpenAISpark = false
        Migration.migrate(config)
        verify(!config.showOpenAISpark)
        compare(writes, 1)
        config.openaiMigrationComplete = false
        config.providerColors = '{"codex":"#111111","openai":"#222222"}'
        Migration.migrate(config)
        compare(JSON.parse(config.providerColors).openai, "#222222")
    }
}
