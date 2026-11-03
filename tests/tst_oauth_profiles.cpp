#include <kodometer/oauth_profiles.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

using Kodometer::OAuthProfiles;

namespace {
QString document(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
QJsonObject record()
{
    return {{"id", "11111111-1111-4111-8111-111111111111"},
            {"name", "Work"},
            {"directory", "/profiles/work"}};
}
QJsonObject config()
{
    return {
        {"version", 1}, {"openai", QJsonArray{record()}}, {"selectedOpenAI", record().value("id")}};
}
} // namespace

class OAuthProfilesTest final : public QObject
{
    Q_OBJECT
  private slots:
    void migratesLegacyCodexProfilesWithoutFallback()
    {
        OAuthProfiles profiles;
        auto legacy = config();
        legacy.insert("codex", legacy.take("openai"));
        legacy.insert("selectedCodex", legacy.take("selectedOpenAI"));
        profiles.setConfiguration(document(legacy));
        QVERIFY(profiles.valid());
        QCOMPARE(profiles.selectedName("openai"), QStringLiteral("Work"));
        QCOMPARE(profiles.selectedDirectory("openai"), QStringLiteral("/profiles/work"));
        QVERIFY(profiles.selectProfile("openai", "default"));
        QVERIFY(!profiles.configuration().contains(QStringLiteral("codex")));
        legacy.insert("openai", QJsonArray{});
        profiles.setConfiguration(document(legacy));
        QVERIFY(!profiles.valid()); // Never pick one of two conflicting registries.
    }

    void managesOnlyNonsecretMetadata()
    {
        OAuthProfiles profiles;
        QSignalSpy changes(&profiles, &OAuthProfiles::configurationChanged);
        QSignalSpy contexts(&profiles, &OAuthProfiles::contextChanged);
        QVERIFY(profiles.valid());
        QVERIFY(profiles.error().isEmpty());
        QCOMPARE(profiles.providers().size(), 3);
        QCOMPARE(profiles.configuration(), QStringLiteral("{}"));
        QCOMPARE(profiles.entries("openai").size(), 1);
        QCOMPARE(profiles.selectedId("openai"), QStringLiteral("default"));
        QVERIFY(profiles.selectedDirectory("openai").isEmpty());
        QVERIFY(profiles.entries("unknown").isEmpty());
        QVERIFY(profiles.contextKey("unknown").isEmpty());
        profiles.setConfiguration("{}");
        QCOMPARE(changes.count(), 0);
        QVERIFY(profiles.addProfile("openai", "  Work  ", "/profiles/./work/"));
        QCOMPARE(profiles.entries("openai").size(), 2);
        QCOMPARE(profiles.selectedName("openai"), QStringLiteral("Work"));
        QCOMPARE(profiles.selectedDirectory("openai"), QStringLiteral("/profiles/work"));
        const QString id = profiles.selectedId("openai");
        QVERIFY(id != QStringLiteral("default"));
        QCOMPARE(contexts.count(), 1);
        QVERIFY(!profiles.configuration().contains(QStringLiteral("accessToken")));
        OAuthProfiles copy;
        copy.setConfiguration(profiles.configuration());
        QCOMPARE(copy.entries("openai"), profiles.entries("openai"));
        QCOMPARE(copy.contextKey("openai"), profiles.contextKey("openai"));
        QVERIFY(profiles.selectProfile("openai", "default"));
        QVERIFY(profiles.selectProfile("openai", "default"));
        QCOMPARE(contexts.count(), 2);
        QVERIFY(profiles.removeProfile("openai", id));
        QCOMPARE(contexts.count(), 2); // Removing an inactive entry does not change credentials.
        QVERIFY(profiles.addProfile("claude", "Personal", "/profiles/claude"));
        QVERIFY(profiles.removeProfile("claude", profiles.selectedId("claude")));
        QCOMPARE(profiles.selectedId("claude"), QStringLiteral("default"));
        QCOMPARE(profiles.entries("claude").size(), 1);
    }

    void rejectsEditsWithoutDamagingExistingConfiguration()
    {
        OAuthProfiles profiles;
        QVERIFY(!profiles.addProfile("unknown", "Work", "/work"));
        QVERIFY(!profiles.selectProfile("unknown", "default"));
        QVERIFY(!profiles.removeProfile("openai", "default"));
        QVERIFY(!profiles.removeProfile("openai", "missing"));
        QVERIFY(!profiles.selectProfile("claude", "missing"));
        QVERIFY(profiles.addProfile("openai", "Work", "/work"));
        const QString original = profiles.configuration();
        for (const auto &entry :
             QList<QPair<QString, QString>>{{"", "/other"},
                                            {QString(65, 'a'), "/other"},
                                            {"Bad\nName", "/other"},
                                            {"Work", "/other"},
                                            {"Other", "/work"},
                                            {"Other", "relative"},
                                            {"Other", "~/work"},
                                            {"Other", "file:///work"},
                                            {"Other", "/bad\npath"},
                                            {"Other", "/" + QString(4096, 'a')}}) {
            QVERIFY(!profiles.addProfile("openai", entry.first, entry.second));
            QVERIFY(!profiles.error().isEmpty());
            QCOMPARE(profiles.configuration(), original);
            QVERIFY(profiles.valid());
        }
        for (int index = 1; index < 8; ++index) {
            QVERIFY(profiles.addProfile("openai", QString::number(index),
                                        "/" + QString::number(index)));
        }
        QVERIFY(!profiles.addProfile("openai", "Too many", "/too-many"));
        QCOMPARE(profiles.entries("openai").size(), 9);
    }

    void rejectsInvalidStoredConfiguration_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("json") << QStringLiteral("{");
        QTest::newRow("array") << QStringLiteral("[]");
        QTest::newRow("oversized") << QString(65537, ' ');
        QTest::newRow("version") << document({{"version", 2}});
        QTest::newRow("unknown-field") << document({{"apiKey", "never accepted"}});
        QTest::newRow("null-list") << document({{"openai", QJsonValue::Null}});
        QTest::newRow("unknown-selection") << document({{"selectedOpenAI", "missing"}});
        QTest::newRow("null-selection") << document({{"selectedClaude", QJsonValue::Null}});
        QTest::newRow("gemini-list-type") << document({{"gemini", true}});
        QTest::newRow("gemini-selection-type") << document({{"selectedGemini", QJsonValue::Null}});
        QTest::newRow("gemini-row-type") << document({{"gemini", QJsonArray{false}}});
        QTest::newRow("bad-row") << document({{"openai", QJsonArray{1}}});
        QJsonArray many;
        for (int i = 0; i < 9; ++i)
            many.append(record());
        QTest::newRow("too-many") << document({{"openai", many}});
        for (const QString &key :
             {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("directory")}) {
            QJsonObject row = record();
            row.remove(key);
            QTest::newRow(qPrintable("missing-" + key)) << document({{"openai", QJsonArray{row}}});
            row.insert(key, 12);
            QTest::newRow(qPrintable("typed-" + key)) << document({{"openai", QJsonArray{row}}});
        }
        QJsonObject badId = record();
        badId.insert("id", "default");
        QTest::newRow("reserved-id") << document({{"claude", QJsonArray{badId}}});
        badId.insert("id", "{11111111-1111-4111-8111-111111111111}");
        QTest::newRow("noncanonical-id") << document({{"openai", QJsonArray{badId}}});
        QJsonObject formatName = record();
        formatName.insert("name", QStringLiteral("Bad\u202eName"));
        QTest::newRow("format-character") << document({{"openai", QJsonArray{formatName}}});
        QJsonObject secret = record();
        secret.insert("refreshToken", "not a supported field");
        QTest::newRow("extra-field") << document({{"openai", QJsonArray{secret}}});
        QTest::newRow("gemini-secret-field") << document({{"gemini", QJsonArray{secret}}});
        QTest::newRow("duplicate") << document({{"openai", QJsonArray{record(), record()}}});
    }

    void rejectsInvalidStoredConfiguration()
    {
        QFETCH(QString, text);
        OAuthProfiles profiles;
        profiles.setConfiguration(document(config()));
        profiles.setConfiguration(text);
        QVERIFY(!profiles.valid());
        QVERIFY(!profiles.error().isEmpty());
        QVERIFY(profiles.entries("openai").isEmpty());
        QVERIFY(profiles.selectedDirectory("openai").isEmpty());
        QVERIFY(!profiles.addProfile("openai", "New", "/new"));
        QVERIFY(!profiles.selectProfile("openai", "default"));
        QVERIFY(!profiles.removeProfile("openai", "missing"));
        profiles.setConfiguration("{}");
        QVERIFY(profiles.valid());
        QVERIFY(profiles.error().isEmpty());
    }

    void separatesCredentialContextFromLabels()
    {
        OAuthProfiles profiles;
        profiles.setConfiguration(document(config()));
        QSignalSpy contexts(&profiles, &OAuthProfiles::contextChanged);
        const QString before = profiles.contextKey("openai");
        QJsonObject row = record();
        row.insert("name", "Renamed");
        QJsonObject data = config();
        data.insert("openai", QJsonArray{row});
        profiles.setConfiguration(document(data));
        QCOMPARE(contexts.count(), 0);
        QCOMPARE(profiles.contextKey("openai"), before);
        row.insert("directory", "/changed");
        data.insert("openai", QJsonArray{row});
        profiles.setConfiguration(document(data));
        QCOMPARE(contexts.count(), 1);
        QVERIFY(profiles.contextKey("openai") != before);
    }

    void removesLaterEntriesAndRejectsUnsupportedRemoval()
    {
        OAuthProfiles profiles;
        QVERIFY(!profiles.removeProfile("unknown", "missing"));
        QVERIFY(profiles.addProfile("openai", "One", "/one"));
        QVERIFY(profiles.addProfile("openai", "Two", "/two"));
        QVERIFY(profiles.removeProfile("openai", profiles.selectedId("openai")));
        QCOMPARE(profiles.entries("openai").size(), 2);
        QCOMPARE(profiles.selectedId("openai"), QStringLiteral("default"));
        QCOMPARE(profiles.entries("claude").size(), 1);
    }

    void managesGeminiWithoutMigratingExistingSelections()
    {
        OAuthProfiles profiles;
        profiles.setConfiguration(document(config()));
        const auto openai = profiles.contextKey("openai");
        QCOMPARE(profiles.selectedId("gemini"), QStringLiteral("default"));
        QCOMPARE(profiles.entries("gemini").size(), 1);
        QVERIFY(profiles.addProfile("gemini", "Work", "/profiles/gemini"));
        const auto id = profiles.selectedId("gemini");
        QCOMPARE(profiles.selectedDirectory("gemini"), QStringLiteral("/profiles/gemini"));
        QCOMPARE(profiles.contextKey("openai"), openai);
        QCOMPARE(QJsonDocument::fromJson(profiles.configuration().toUtf8())
                     .object()
                     .value("selectedGemini")
                     .toString(),
                 id);
        OAuthProfiles copy;
        copy.setConfiguration(profiles.configuration());
        QCOMPARE(copy.contextKey("gemini"), profiles.contextKey("gemini"));
        QVERIFY(copy.selectProfile("gemini", "default"));
        QVERIFY(copy.removeProfile("gemini", id));
        QCOMPARE(copy.contextKey("openai"), openai);
        for (int i = 1; i < 8; ++i)
            QVERIFY(
                profiles.addProfile("gemini", QString::number(i), "/gemini/" + QString::number(i)));
        QVERIFY(!profiles.addProfile("gemini", "Nine", "/nine"));
        profiles.setConfiguration(document({{"selectedGemini", "missing"}}));
        QVERIFY(!profiles.valid());
        QVERIFY(profiles.entries("gemini").isEmpty());
        QCOMPARE(profiles.contextKey("gemini"), QStringLiteral("invalid"));
        QVERIFY(profiles.error().contains("Gemini"));
    }

    void acceptsOnlyLocalFolderUrls()
    {
        OAuthProfiles profiles;
        QCOMPARE(profiles.localDirectory(QUrl::fromLocalFile("/work space/é")),
                 QStringLiteral("/work space/é"));
        QVERIFY(profiles.localDirectory(QUrl("https://example.test/work")).isEmpty());
        QVERIFY(profiles.localDirectory(QUrl("file://remote/work")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(OAuthProfilesTest)
#include "tst_oauth_profiles.moc"
