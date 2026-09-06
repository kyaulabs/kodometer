#include <kodometer/openrouter_usage_parser.hpp>

#include <QTimeZone>
#include <QtTest>

using Kodometer::OpenRouterActivity;
using Kodometer::OpenRouterActivityEntry;
using Kodometer::OpenRouterKeyUsage;
using Kodometer::OpenRouterUsageParser;

namespace {

const QDateTime UpdatedAt =
    QDateTime::fromString(QStringLiteral("2027-01-15T08:00:00Z"), Qt::ISODate);

} // namespace

class OpenRouterUsageParserTest final : public QObject
{
    Q_OBJECT

  private slots:
    void parsesCreditsAndKeyQuota();
    void mapsCreditsWithoutOptionalEnrichment();
    void mapsKeyQuotaFallbacks();
    void parsesAndMergesActivity();
    void validatesActivityBoundsAndDuplicates();
    void rejectsMalformedCredits_data();
    void rejectsMalformedCredits();
    void rejectsMalformedKey_data();
    void rejectsMalformedKey();
    void rejectsMalformedActivity_data();
    void rejectsMalformedActivity();
};

void OpenRouterUsageParserTest::parsesCreditsAndKeyQuota()
{
    QString error;
    const auto credits = OpenRouterUsageParser::parseCredits(
        R"({"data":{"total_credits":100,"total_usage":40}})", &error);
    const auto key = OpenRouterUsageParser::parseKey(
        R"({"data":{"limit":20,"limit_remaining":15,"limit_reset":"monthly","usage":5,"usage_daily":1,"usage_weekly":2,"usage_monthly":4,"rate_limit":{"requests":120,"interval":"10s"}}})",
        &error);

    QVERIFY(credits.has_value());
    QVERIFY(key.has_value());
    QCOMPARE(credits->balance, 60.0);
    QCOMPARE(key->limit.value(), 20.0);
    QCOMPARE(key->limitRemaining.value(), 15.0);

    const QVariantMap provider = OpenRouterUsageParser::provider(
        *credits, key, {}, QStringLiteral("Management API key not configured"), UpdatedAt);
    QCOMPARE(provider.value(QStringLiteral("id")).toString(), QStringLiteral("openrouter"));
    QCOMPARE(provider.value(QStringLiteral("name")).toString(), QStringLiteral("OpenRouter"));
    QCOMPARE(provider.value(QStringLiteral("identity")).toMap().value(QStringLiteral("plan")),
             QStringLiteral("Balance: $60.00"));
    const QVariantList windows = provider.value(QStringLiteral("windows")).toList();
    QCOMPARE(windows.size(), 1);
    QCOMPARE(windows.first().toMap().value(QStringLiteral("usedPercent")).toDouble(), 25.0);
    QCOMPARE(windows.first().toMap().value(QStringLiteral("remainingPercent")).toDouble(), 75.0);
    const QVariantMap cost = provider.value(QStringLiteral("cost")).toMap();
    QCOMPARE(cost.value(QStringLiteral("balanceUSD")).toDouble(), 60.0);
    QCOMPARE(cost.value(QStringLiteral("usedUSD")).toDouble(), 40.0);
    const QVariantList details = provider.value(QStringLiteral("details")).toList();
    QCOMPARE(details.size(), 3);
    QCOMPARE(details.at(1).toMap().value(QStringLiteral("rows")).toList().size(), 8);
    QVERIFY(error.isEmpty());
}

void OpenRouterUsageParserTest::mapsCreditsWithoutOptionalEnrichment()
{
    const auto credits =
        OpenRouterUsageParser::parseCredits(R"({"data":{"total_credits":5,"total_usage":9}})");
    QVERIFY(credits.has_value());
    QCOMPARE(credits->balance, 0.0);

    const QVariantMap provider = OpenRouterUsageParser::provider(
        *credits, {}, QStringLiteral("Request timed out"), {}, UpdatedAt);
    QVERIFY(provider.value(QStringLiteral("windows")).toList().isEmpty());
    QCOMPARE(provider.value(QStringLiteral("dataConfidence")).toString(), QStringLiteral("exact"));
    const QVariantList sections = provider.value(QStringLiteral("details")).toList();
    QCOMPARE(sections.size(), 2);
    const QVariantMap unavailable =
        sections.at(1).toMap().value(QStringLiteral("rows")).toList().first().toMap();
    QCOMPARE(unavailable.value(QStringLiteral("value")), QStringLiteral("Unavailable right now"));
    QCOMPARE(unavailable.value(QStringLiteral("secondaryValue")),
             QStringLiteral("Request timed out"));
}

void OpenRouterUsageParserTest::mapsKeyQuotaFallbacks()
{
    const auto credits =
        OpenRouterUsageParser::parseCredits(R"({"data":{"total_credits":10,"total_usage":2}})");
    QVERIFY(credits.has_value());

    OpenRouterKeyUsage key;
    key.limit = 10.0;
    key.limitRemaining = 12.0;
    QVariantMap provider = OpenRouterUsageParser::provider(*credits, key, {}, {}, UpdatedAt);
    QCOMPARE(provider.value("windows").toList().first().toMap().value("usedPercent"), 0.0);

    key.limitRemaining.reset();
    key.limitReset = QStringLiteral("daily");
    key.daily = 2.0;
    provider = OpenRouterUsageParser::provider(*credits, key, {}, {}, UpdatedAt);
    QCOMPARE(provider.value("windows").toList().first().toMap().value("usedPercent"), 20.0);

    key.limitReset = QStringLiteral("weekly");
    key.weekly = 3.0;
    provider = OpenRouterUsageParser::provider(*credits, key, {}, {}, UpdatedAt);
    QCOMPARE(provider.value("windows").toList().first().toMap().value("usedPercent"), 30.0);

    key.limitReset = QStringLiteral("monthly");
    key.monthly = 4.0;
    provider = OpenRouterUsageParser::provider(*credits, key, {}, {}, UpdatedAt);
    QCOMPARE(provider.value("windows").toList().first().toMap().value("usedPercent"), 40.0);

    key.limitReset = QStringLiteral("never");
    key.usage = 15.0;
    provider = OpenRouterUsageParser::provider(*credits, key, {}, {}, UpdatedAt);
    QCOMPARE(provider.value("windows").toList().first().toMap().value("usedPercent"), 100.0);

    key.limit = 0.0;
    provider = OpenRouterUsageParser::provider(*credits, key, {}, {}, UpdatedAt);
    QVERIFY(provider.value("windows").toList().isEmpty());
    QCOMPARE(provider.value("details")
                 .toList()
                 .at(1)
                 .toMap()
                 .value("rows")
                 .toList()
                 .first()
                 .toMap()
                 .value("value"),
             QStringLiteral("No limit configured"));
}

void OpenRouterUsageParserTest::parsesAndMergesActivity()
{
    QString error;
    const auto first = OpenRouterUsageParser::parseActivity(
        R"({"data":[{"date":"2027-01-14","model_permaslug":"openai/gpt","endpoint_id":"one","provider_name":"OpenAI","workspace_id":"work","prompt_tokens":100,"completion_tokens":40,"reasoning_tokens":10,"requests":2,"usage":1.25,"byok_usage_inference":0.25},{"date":"2026-12-01","prompt_tokens":1,"completion_tokens":1,"requests":1,"usage":9}]})",
        UpdatedAt, &error);
    const auto second = OpenRouterUsageParser::parseActivity(
        R"({"data":[{"date":"2027-01-14 12:00:00","model_permaslug":"openai/gpt","endpoint_id":"one","provider_name":"OpenAI","workspace_id":"work","prompt_tokens":100,"completion_tokens":40,"reasoning_tokens":10,"requests":2,"usage":1.25,"byok_usage_inference":0.25},{"date":"2027-01-13","model":"anthropic/claude","prompt_tokens":20,"completion_tokens":5,"requests":1,"usage":0.5}]})",
        UpdatedAt, &error);

    QVERIFY(first.has_value());
    QVERIFY(second.has_value());
    const auto merged = OpenRouterUsageParser::mergeActivity(*first, *second, &error);
    QVERIFY(merged.has_value());
    QCOMPARE(merged->entries.size(), 2);
    QCOMPARE(merged->totalCost, 2.0);
    QCOMPARE(merged->inputTokens, 120);
    QCOMPARE(merged->outputTokens, 45);
    QCOMPARE(merged->reasoningTokens, 10);
    QCOMPARE(merged->requests, 3);

    const auto credits =
        OpenRouterUsageParser::parseCredits(R"({"data":{"total_credits":10,"total_usage":2}})");
    const QVariantMap provider =
        OpenRouterUsageParser::provider(*credits, {}, {}, {}, UpdatedAt, merged);
    const QVariantMap cost = provider.value(QStringLiteral("cost")).toMap();
    QCOMPARE(cost.value(QStringLiteral("last30DaysUSD")).toDouble(), 2.0);
    QCOMPARE(cost.value(QStringLiteral("daily")).toList().size(), 2);
    QCOMPARE(cost.value(QStringLiteral("historyEndDate")).toString(), QStringLiteral("2027-01-14"));
    QCOMPARE(cost.value(QStringLiteral("historyIncludesCurrentDay")), QVariant(false));
    QCOMPARE(provider.value(QStringLiteral("dataConfidence")).toString(), QStringLiteral("exact"));
    QVERIFY(error.isEmpty());
}

void OpenRouterUsageParserTest::validatesActivityBoundsAndDuplicates()
{
    QByteArray oversized = R"({"data":[)";
    for (int index = 0; index < 20'001; ++index) {
        if (index > 0) {
            oversized.append(',');
        }
        oversized.append("{}");
    }
    oversized.append("]}");
    QString error;
    QVERIFY(!OpenRouterUsageParser::parseActivity(oversized, UpdatedAt, &error).has_value());
    QCOMPARE(error, QStringLiteral("OpenRouter activity API returned malformed data"));

    OpenRouterActivityEntry first;
    first.date = QStringLiteral("2027-01-14");
    first.model = QStringLiteral("model");
    first.inputTokens = 1;
    first.outputTokens = 1;
    first.requests = 1;
    first.cost = 1.0;
    OpenRouterActivityEntry conflict = first;
    conflict.cost = 2.0;
    OpenRouterActivity left;
    left.entries.append(first);
    OpenRouterActivity right;
    right.entries.append(conflict);
    QVERIFY(!OpenRouterUsageParser::mergeActivity(left, right, &error).has_value());
    QCOMPARE(error, QStringLiteral("OpenRouter activity contains conflicting duplicate rows"));

    OpenRouterActivityEntry maximum = first;
    maximum.inputTokens = 9'007'199'254'740'991LL;
    OpenRouterActivityEntry overflow = first;
    overflow.model = QStringLiteral("other");
    QVERIFY(!OpenRouterUsageParser::mergeActivity(OpenRouterActivity{{maximum}},
                                                  OpenRouterActivity{{overflow}}, &error)
                 .has_value());
    QCOMPARE(error,
             QStringLiteral("OpenRouter activity aggregate exceeded the safe integer range"));
}

void OpenRouterUsageParserTest::rejectsMalformedCredits_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<QString>("message");
    QTest::newRow("json") << QByteArray("{")
                          << QStringLiteral("OpenRouter credits API returned invalid JSON");
    QTest::newRow("root") << QByteArray("[]")
                          << QStringLiteral("OpenRouter credits API returned an invalid object");
    QTest::newRow("data") << QByteArray(R"({"data":[]})")
                          << QStringLiteral("OpenRouter credits API returned malformed data");
    QTest::newRow("credits") << QByteArray(R"({"data":{"total_credits":"5","total_usage":1}})")
                             << QStringLiteral(
                                    "OpenRouter total_credits must be a finite nonnegative number");
    QTest::newRow("usage") << QByteArray(R"({"data":{"total_credits":5,"total_usage":-1}})")
                           << QStringLiteral(
                                  "OpenRouter total_usage must be a finite nonnegative number");
}

void OpenRouterUsageParserTest::rejectsMalformedCredits()
{
    QFETCH(QByteArray, payload);
    QFETCH(QString, message);
    QString error;
    QVERIFY(!OpenRouterUsageParser::parseCredits(payload, &error).has_value());
    QCOMPARE(error, message);
}

void OpenRouterUsageParserTest::rejectsMalformedKey_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<QString>("message");
    QTest::newRow("json") << QByteArray("{")
                          << QStringLiteral("OpenRouter key API returned invalid JSON");
    QTest::newRow("root") << QByteArray("[]")
                          << QStringLiteral("OpenRouter key API returned an invalid object");
    QTest::newRow("data") << QByteArray(R"({"data":[]})")
                          << QStringLiteral("OpenRouter key API returned malformed data");
    QTest::newRow("number") << QByteArray(R"({"data":{"limit":"20"}})")
                            << QStringLiteral(
                                   "OpenRouter key.limit must be a finite nonnegative number");
    QTest::newRow("reset") << QByteArray(R"({"data":{"limit_reset":4}})")
                           << QStringLiteral("OpenRouter key.limit_reset must be a string");
    QTest::newRow("rate") << QByteArray(
                                 R"({"data":{"rate_limit":{"requests":1.5,"interval":"10s"}}})")
                          << QStringLiteral("OpenRouter key.rate_limit is invalid");
    QTest::newRow("rate interval")
        << QByteArray(R"({"data":{"rate_limit":{"requests":1,"interval":""}}})")
        << QStringLiteral("OpenRouter key.rate_limit is invalid");
}

void OpenRouterUsageParserTest::rejectsMalformedKey()
{
    QFETCH(QByteArray, payload);
    QFETCH(QString, message);
    QString error;
    QVERIFY(!OpenRouterUsageParser::parseKey(payload, &error).has_value());
    QCOMPARE(error, message);
}

void OpenRouterUsageParserTest::rejectsMalformedActivity_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<QString>("message");
    QTest::newRow("json") << QByteArray("{")
                          << QStringLiteral("OpenRouter activity API returned invalid JSON");
    QTest::newRow("root") << QByteArray("[]")
                          << QStringLiteral("OpenRouter activity API returned an invalid object");
    QTest::newRow("data") << QByteArray(R"({"data":{}})")
                          << QStringLiteral("OpenRouter activity API returned malformed data");
    QTest::newRow("row") << QByteArray(R"({"data":[[]]})")
                         << QStringLiteral("OpenRouter activity row must be an object");
    QTest::newRow("date") << QByteArray(R"({"data":[{"date":"tomorrow"}]})")
                          << QStringLiteral("OpenRouter activity date is invalid");
    QTest::newRow("calendar date") << QByteArray(R"({"data":[{"date":"2027-02-31"}]})")
                                   << QStringLiteral("OpenRouter activity date is invalid");
    QTest::newRow("model")
        << QByteArray(
               R"({"data":[{"date":"2027-01-14","model":"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklm","prompt_tokens":1,"completion_tokens":1,"requests":1,"usage":1}]})")
        << QStringLiteral("OpenRouter activity model exceeds 64 characters");
    QTest::newRow("future")
        << QByteArray(
               R"({"data":[{"date":"2027-01-15","prompt_tokens":1,"completion_tokens":1,"requests":1,"usage":1}]})")
        << QStringLiteral("OpenRouter activity date must be a completed UTC day");
    QTest::newRow("tokens")
        << QByteArray(
               R"({"data":[{"date":"2027-01-14","prompt_tokens":-1,"completion_tokens":1,"requests":1,"usage":1}]})")
        << QStringLiteral("OpenRouter activity prompt_tokens must be a nonnegative integer");
    QTest::newRow("reasoning")
        << QByteArray(
               R"({"data":[{"date":"2027-01-14","prompt_tokens":1,"completion_tokens":1,"reasoning_tokens":2,"requests":1,"usage":1}]})")
        << QStringLiteral("OpenRouter activity reasoning_tokens exceeds completion_tokens");
    QTest::newRow("token total")
        << QByteArray(
               R"({"data":[{"date":"2027-01-14","prompt_tokens":9007199254740991,"completion_tokens":1,"requests":1,"usage":1}]})")
        << QStringLiteral("OpenRouter activity token total overflowed");
    QTest::newRow("cost")
        << QByteArray(
               R"({"data":[{"date":"2027-01-14","prompt_tokens":1,"completion_tokens":1,"requests":1,"usage":-1}]})")
        << QStringLiteral("OpenRouter activity usage must be a finite nonnegative number");
    QTest::newRow("cost aggregate")
        << QByteArray(
               R"({"data":[{"date":"2027-01-14","prompt_tokens":1,"completion_tokens":1,"requests":1,"usage":1e308,"byok_usage_inference":1e308}]})")
        << QStringLiteral("OpenRouter activity spend aggregate overflowed");
}

void OpenRouterUsageParserTest::rejectsMalformedActivity()
{
    QFETCH(QByteArray, payload);
    QFETCH(QString, message);
    QString error;
    QVERIFY(!OpenRouterUsageParser::parseActivity(payload, UpdatedAt, &error).has_value());
    QCOMPARE(error, message);
}

QTEST_GUILESS_MAIN(OpenRouterUsageParserTest)

#include "tst_openrouter_usage_parser.moc"
