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
        {"version", 1}, {"codex", QJsonArray{record()}}, {"selectedCodex", record().value("id")}};
}
} // namespace

class OAuthProfilesTest final : public QObject
{
    Q_OBJECT
  private slots:
    void managesOnlyNonsecretMetadata()
    {
        OAuthProfiles profiles;
        QSignalSpy changes(&profiles, &OAuthProfiles::configurationChanged);
        QSignalSpy contexts(&profiles, &OAuthProfiles::contextChanged);
        QVERIFY(profiles.valid());
        QVERIFY(profiles.error().isEmpty());
        QCOMPARE(profiles.providers().size(), 3);
        QCOMPARE(profiles.configuration(), QStringLiteral("{}"));
        QCOMPARE(profiles.entries("codex").size(), 1);
        QCOMPARE(profiles.selectedId("codex"), QStringLiteral("default"));
        QVERIFY(profiles.selectedDirectory("codex").isEmpty());
        QVERIFY(profiles.entries("unknown").isEmpty());
        QVERIFY(profiles.contextKey("unknown").isEmpty());
        profiles.setConfiguration("{}");
        QCOMPARE(changes.count(), 0);
        QVERIFY(profiles.addProfile("codex", "  Work  ", "/profiles/./work/"));
        QCOMPARE(profiles.entries("codex").size(), 2);
        QCOMPARE(profiles.selectedName("codex"), QStringLiteral("Work"));
        QCOMPARE(profiles.selectedDirectory("codex"), QStringLiteral("/profiles/work"));
        const QString id = profiles.selectedId("codex");
        QVERIFY(id != QStringLiteral("default"));
        QCOMPARE(contexts.count(), 1);
        QVERIFY(!profiles.configuration().contains(QStringLiteral("accessToken")));
        OAuthProfiles copy;
        copy.setConfiguration(profiles.configuration());
        QCOMPARE(copy.entries("codex"), profiles.entries("codex"));
        QCOMPARE(copy.contextKey("codex"), profiles.contextKey("codex"));
        QVERIFY(profiles.selectProfile("codex", "default"));
        QVERIFY(profiles.selectProfile("codex", "default"));
        QCOMPARE(contexts.count(), 2);
        QVERIFY(profiles.removeProfile("codex", id));
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
        QVERIFY(!profiles.removeProfile("codex", "default"));
        QVERIFY(!profiles.removeProfile("codex", "missing"));
        QVERIFY(!profiles.selectProfile("claude", "missing"));
        QVERIFY(profiles.addProfile("codex", "Work", "/work"));
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
            QVERIFY(!profiles.addProfile("codex", entry.first, entry.second));
            QVERIFY(!profiles.error().isEmpty());
            QCOMPARE(profiles.configuration(), original);
            QVERIFY(profiles.valid());
        }
        for (int index = 1; index < 8; ++index) {
            QVERIFY(
                profiles.addProfile("codex", QString::number(index), "/" + QString::number(index)));
        }
        QVERIFY(!profiles.addProfile("codex", "Too many", "/too-many"));
        QCOMPARE(profiles.entries("codex").size(), 9);
    }

    void rejectsInvalidStoredConfiguration_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("json") << QStringLiteral("{");
        QTest::newRow("array") << QStringLiteral("[]");
        QTest::newRow("oversized") << QString(65537, ' ');
        QTest::newRow("version") << document({{"version", 2}});
        QTest::newRow("unknown-field") << document({{"apiKey", "never accepted"}});
        QTest::newRow("null-list") << document({{"codex", QJsonValue::Null}});
        QTest::newRow("unknown-selection") << document({{"selectedCodex", "missing"}});
        QTest::newRow("null-selection") << document({{"selectedClaude", QJsonValue::Null}});
        QTest::newRow("bad-row") << document({{"codex", QJsonArray{1}}});
        QJsonArray many;
        for (int i = 0; i < 9; ++i)
            many.append(record());
        QTest::newRow("too-many") << document({{"codex", many}});
        for (const QString &key :
             {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("directory")}) {
            QJsonObject row = record();
            row.remove(key);
            QTest::newRow(qPrintable("missing-" + key)) << document({{"codex", QJsonArray{row}}});
            row.insert(key, 12);
            QTest::newRow(qPrintable("typed-" + key)) << document({{"codex", QJsonArray{row}}});
        }
        QJsonObject badId = record();
        badId.insert("id", "default");
        QTest::newRow("reserved-id") << document({{"claude", QJsonArray{badId}}});
        badId.insert("id", "{11111111-1111-4111-8111-111111111111}");
        QTest::newRow("noncanonical-id") << document({{"codex", QJsonArray{badId}}});
        QJsonObject formatName = record();
        formatName.insert("name", QStringLiteral("Bad\u202eName"));
        QTest::newRow("format-character") << document({{"codex", QJsonArray{formatName}}});
        QJsonObject secret = record();
        secret.insert("refreshToken", "not a supported field");
        QTest::newRow("extra-field") << document({{"codex", QJsonArray{secret}}});
        QTest::newRow("duplicate") << document({{"codex", QJsonArray{record(), record()}}});
    }

    void rejectsInvalidStoredConfiguration()
    {
        QFETCH(QString, text);
        OAuthProfiles profiles;
        profiles.setConfiguration(document(config()));
        profiles.setConfiguration(text);
        QVERIFY(!profiles.valid());
        QVERIFY(!profiles.error().isEmpty());
        QVERIFY(profiles.entries("codex").isEmpty());
        QVERIFY(profiles.selectedDirectory("codex").isEmpty());
        QVERIFY(!profiles.addProfile("codex", "New", "/new"));
        QVERIFY(!profiles.selectProfile("codex", "default"));
        QVERIFY(!profiles.removeProfile("codex", "missing"));
        profiles.setConfiguration("{}");
        QVERIFY(profiles.valid());
        QVERIFY(profiles.error().isEmpty());
    }

    void separatesCredentialContextFromLabels()
    {
        OAuthProfiles profiles;
        profiles.setConfiguration(document(config()));
        QSignalSpy contexts(&profiles, &OAuthProfiles::contextChanged);
        const QString before = profiles.contextKey("codex");
        QJsonObject row = record();
        row.insert("name", "Renamed");
        QJsonObject data = config();
        data.insert("codex", QJsonArray{row});
        profiles.setConfiguration(document(data));
        QCOMPARE(contexts.count(), 0);
        QCOMPARE(profiles.contextKey("codex"), before);
        row.insert("directory", "/changed");
        data.insert("codex", QJsonArray{row});
        profiles.setConfiguration(document(data));
        QCOMPARE(contexts.count(), 1);
        QVERIFY(profiles.contextKey("codex") != before);
    }

    void removesLaterEntriesAndRejectsUnsupportedRemoval()
    {
        OAuthProfiles profiles;
        QVERIFY(!profiles.removeProfile("unknown", "missing"));
        QVERIFY(profiles.addProfile("codex", "One", "/one"));
        QVERIFY(profiles.addProfile("codex", "Two", "/two"));
        QVERIFY(profiles.removeProfile("codex", profiles.selectedId("codex")));
        QCOMPARE(profiles.entries("codex").size(), 2);
        QCOMPARE(profiles.selectedId("codex"), QStringLiteral("default"));
        QCOMPARE(profiles.entries("claude").size(), 1);
    }

    void managesGeminiWithoutMigratingExistingSelections()
    {
        OAuthProfiles profiles;
        profiles.setConfiguration(document(config()));
        const auto codex = profiles.contextKey("codex");
        QCOMPARE(profiles.selectedId("gemini"), QStringLiteral("default"));
        QCOMPARE(profiles.entries("gemini").size(), 1);
        QVERIFY(profiles.addProfile("gemini", "Work", "/profiles/gemini"));
        const auto id = profiles.selectedId("gemini");
        QCOMPARE(profiles.selectedDirectory("gemini"), QStringLiteral("/profiles/gemini"));
        QCOMPARE(profiles.contextKey("codex"), codex);
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
        QCOMPARE(copy.contextKey("codex"), codex);
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
