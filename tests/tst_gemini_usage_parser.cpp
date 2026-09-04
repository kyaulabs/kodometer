#include <kodometer/gemini_usage_parser.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include <QtTest>

using Kodometer::GeminiCodeAssistStatus;
using Kodometer::GeminiCredentials;
using Kodometer::GeminiUsageParser;

class GeminiUsageParserTest final : public QObject
{
    Q_OBJECT

  private slots:
    void mapsModelQuotaTiersAndIdentity();
    void keepsMostConstrainedModelPerTier();
    void mapsCodeAssistStatusAndPlans();
    void detectsMigrationAndProjects();
    void rejectsInvalidResponses();
};

void GeminiUsageParserTest::mapsModelQuotaTiersAndIdentity()
{
    GeminiCredentials credentials;
    credentials.email = QStringLiteral("person@example.com");
    GeminiCodeAssistStatus status;
    status.tier = QStringLiteral("standard-tier");
    status.plan = QStringLiteral("Gemini Code Assist Enterprise");
    const QDateTime updatedAt = QDateTime::fromSecsSinceEpoch(1'700'000'000, QTimeZone::UTC);
    QString error;

    const auto provider = GeminiUsageParser::parseQuota(
        R"({"buckets":[
          {"modelId":"gemini-2.5-pro","remainingFraction":0.72,"resetTime":"2026-09-05T12:30:00Z","tokenType":"INPUT"},
          {"modelId":"gemini-2.5-flash","remainingFraction":0.4,"resetTime":"2026-09-05T13:00:00.123Z"},
          {"modelId":"gemini-2.5-flash-lite","remainingFraction":0.9},
          {"modelId":"unrelated","remainingFraction":0.1}
        ]})",
        credentials, status, updatedAt, &error);

    QVERIFY2(provider.has_value(), qPrintable(error));
    QCOMPARE(provider->value(QStringLiteral("id")).toString(), QStringLiteral("gemini"));
    QCOMPARE(provider->value(QStringLiteral("name")).toString(), QStringLiteral("Gemini"));
    QCOMPARE(provider->value(QStringLiteral("source")).toString(), QStringLiteral("oauth"));
    QCOMPARE(provider->value(QStringLiteral("updatedAt")).toString(),
             QStringLiteral("2023-11-14T22:13:20.000Z"));
    const QVariantMap identity = provider->value(QStringLiteral("identity")).toMap();
    QCOMPARE(identity.value(QStringLiteral("accountEmail")).toString(),
             QStringLiteral("redacted@example.com"));
    QCOMPARE(identity.value(QStringLiteral("plan")).toString(),
             QStringLiteral("Gemini Code Assist Enterprise"));

    const QVariantList windows = provider->value(QStringLiteral("windows")).toList();
    QCOMPARE(windows.size(), 3);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("model-pro-daily"));
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Pro"));
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("usedPercent")).toDouble(), 28.0);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("remainingPercent")).toDouble(), 72.0);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("windowSeconds")).toInt(), 86400);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("resetAt")).toString(),
             QStringLiteral("2026-09-05T12:30:00.000Z"));
    QCOMPARE(windows.at(1).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("model-flash-daily"));
    QCOMPARE(windows.at(2).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("model-flash-lite-daily"));
    QVERIFY(!windows.at(2).toMap().contains(QStringLiteral("resetAt")));
    QCOMPARE(provider->value(QStringLiteral("display")).toMap().value(QStringLiteral("sortKey"))
                 .toInt(),
             20);
}

void GeminiUsageParserTest::keepsMostConstrainedModelPerTier()
{
    GeminiCredentials credentials;
    GeminiCodeAssistStatus status;
    QString error;
    const auto provider = GeminiUsageParser::parseQuota(
        R"({"buckets":[
          {"modelId":"gemini-2.5-pro","remainingFraction":0.8,"resetTime":"2026-09-06T00:00:00Z"},
          {"modelId":"gemini-2.5-pro","remainingFraction":0.2,"resetTime":"2026-09-05T00:00:00Z"},
          {"modelId":"gemini-3-pro-preview","remainingFraction":1.4},
          {"modelId":"gemini-3-flash","remainingFraction":-0.2}
        ]})",
        credentials, status, QDateTime::currentDateTimeUtc(), &error);

    QVERIFY2(provider.has_value(), qPrintable(error));
    const QVariantList windows = provider->value(QStringLiteral("windows")).toList();
    QCOMPARE(windows.size(), 2);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("remainingPercent")).toDouble(), 20.0);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("resetAt")).toString(),
             QStringLiteral("2026-09-05T00:00:00.000Z"));
    QCOMPARE(windows.at(1).toMap().value(QStringLiteral("remainingPercent")).toDouble(), 0.0);
    QVERIFY(provider->value(QStringLiteral("identity")).toMap().isEmpty());
}

void GeminiUsageParserTest::mapsCodeAssistStatusAndPlans()
{
    QString error;
    auto status = GeminiUsageParser::parseCodeAssist(
        R"({"cloudaicompanionProject":{"projectId":" project-1 "},"currentTier":{"id":"free-tier"},"paidTier":{"name":" Gemini Code Assist Standard "}})",
        {}, &error);
    QVERIFY2(status.has_value(), qPrintable(error));
    QCOMPARE(status->projectId, QStringLiteral("project-1"));
    QCOMPARE(status->tier, QStringLiteral("free-tier"));
    QCOMPARE(status->plan, QStringLiteral("Gemini Code Assist Standard"));

    status = GeminiUsageParser::parseCodeAssist(
        R"({"cloudaicompanionProject":"workspace-project","currentTier":{"id":"free-tier"}})",
        QStringLiteral("example.edu"), &error);
    QVERIFY(status.has_value());
    QCOMPARE(status->plan, QStringLiteral("Workspace"));

    status = GeminiUsageParser::parseCodeAssist(R"({"currentTier":{"id":"free-tier"}})", {},
                                                &error);
    QCOMPARE(status->plan, QStringLiteral("Free"));
    status = GeminiUsageParser::parseCodeAssist(
        R"({"currentTier":{"id":"standard-tier"}})", {}, &error);
    QCOMPARE(status->plan, QStringLiteral("Paid"));
    status = GeminiUsageParser::parseCodeAssist(R"({"currentTier":{"id":"legacy-tier"}})", {},
                                                &error);
    QCOMPARE(status->plan, QStringLiteral("Legacy"));
    status = GeminiUsageParser::parseCodeAssist(R"({"currentTier":{"id":"future-tier"}})", {},
                                                &error);
    QVERIFY(status->plan.isEmpty());
}

void GeminiUsageParserTest::detectsMigrationAndProjects()
{
    QVERIFY(GeminiUsageParser::isConsumerTierDeprecation(
        R"({"error":"unsupported_client"})"));
    QVERIFY(GeminiUsageParser::isConsumerTierDeprecation(
        "Gemini Code Assist is no longer supported for this account"));
    QVERIFY(GeminiUsageParser::isConsumerTierDeprecation(
        "Migrate Gemini usage to Antigravity"));
    QVERIFY(!GeminiUsageParser::isConsumerTierDeprecation("permission denied"));

    QString error;
    const auto unsupported = GeminiUsageParser::parseCodeAssist(
        R"({"ineligibleTiers":[{"reasonCode":"UNSUPPORTED_CLIENT"}]})", {}, &error);
    QVERIFY2(unsupported.has_value(), qPrintable(error));
    QVERIFY(unsupported->consumerClientUnsupported);
    const auto workspace = GeminiUsageParser::parseCodeAssist(
        R"({"currentTier":{"id":"free-tier"},"ineligibleTiers":[{"reasonMessage":"unsupported_client"}]})",
        QStringLiteral("example.com"), &error);
    QVERIFY(!workspace->consumerClientUnsupported);
    const auto paid = GeminiUsageParser::parseCodeAssist(
        R"({"paidTier":{"name":"Enterprise"},"ineligibleTiers":[{"reasonCode":"UNSUPPORTED_CLIENT"}]})",
        {}, &error);
    QVERIFY(!paid->consumerClientUnsupported);

    QCOMPARE(GeminiUsageParser::discoverProject(
                 R"({"projects":[{"projectId":"other"},{"projectId":"gen-lang-client-123"}]})"),
             QStringLiteral("gen-lang-client-123"));
    QCOMPARE(GeminiUsageParser::discoverProject(
                 R"({"projects":[{"projectId":"labelled","labels":{"generative-language":"yes"}}]})"),
             QStringLiteral("labelled"));
    QVERIFY(GeminiUsageParser::discoverProject("{").isEmpty());
}

void GeminiUsageParserTest::rejectsInvalidResponses()
{
    GeminiCredentials credentials;
    GeminiCodeAssistStatus status;
    QString error;

    QVERIFY(!GeminiUsageParser::parseQuota("{", credentials, status,
                                           QDateTime::currentDateTimeUtc(), &error));
    QCOMPARE(error, QStringLiteral("Gemini quota API returned invalid JSON"));
    QVERIFY(!GeminiUsageParser::parseQuota("[]", credentials, status,
                                           QDateTime::currentDateTimeUtc(), &error));
    QCOMPARE(error, QStringLiteral("Gemini quota API returned an invalid object"));
    QVERIFY(!GeminiUsageParser::parseQuota(R"({"buckets":[]})", credentials, status,
                                           QDateTime::currentDateTimeUtc(), &error));
    QCOMPARE(error, QStringLiteral("Gemini quota API returned no usable model limits"));
    QVERIFY(!GeminiUsageParser::parseQuota(
        R"({"buckets":[{}, {"modelId":"gemini-pro","remainingFraction":"many"}, {"modelId":"other","remainingFraction":0.5}]})",
        credentials, status, QDateTime::currentDateTimeUtc(), &error));
    QCOMPARE(error, QStringLiteral("Gemini quota API returned no usable model limits"));

    QVERIFY(!GeminiUsageParser::parseCodeAssist("{", {}, &error));
    QCOMPARE(error, QStringLiteral("Gemini Code Assist API returned invalid JSON"));
    QVERIFY(!GeminiUsageParser::parseCodeAssist("[]", {}, &error));
    QCOMPARE(error, QStringLiteral("Gemini Code Assist API returned an invalid object"));
}

QTEST_GUILESS_MAIN(GeminiUsageParserTest)

#include "tst_gemini_usage_parser.moc"
