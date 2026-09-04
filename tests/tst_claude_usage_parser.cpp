#include <kodometer/claude_usage_parser.hpp>

#include <QDateTime>
#include <QTimeZone>
#include <QtTest>

using Kodometer::ClaudeCredentials;
using Kodometer::ClaudeUsageParser;

class ClaudeUsageParserTest final : public QObject
{
    Q_OBJECT

  private slots:
    void mapsUsageResponse();
    void mapsSpendLimitWithoutSession();
    void toleratesOptionalAndMalformedWindows();
    void rejectsInvalidResponses();
    void formatsPlans_data();
    void formatsPlans();
};

void ClaudeUsageParserTest::mapsUsageResponse()
{
    const QByteArray payload = R"({
        "five_hour":{"utilization":28.5,"resets_at":"2026-09-04T10:00:00Z"},
        "seven_day":{"utilization":41,"resets_at":"2026-09-10T10:00:00.000Z"},
        "seven_day_sonnet":{"utilization":12,"resets_at":"2026-09-10T10:00:00Z"},
        "seven_day_opus":{"utilization":7,"resets_at":"2026-09-10T10:00:00Z"},
        "seven_day_cowork":{"utilization":3,"resets_at":"2026-09-05T10:00:00Z"},
        "limits":[
            {"kind":"weekly_scoped","group":"weekly","percent":17,
             "resets_at":"2026-09-10T10:00:00Z",
             "scope":{"model":{"id":"claude-fable","display_name":"Fable"}}},
            {"kind":"weekly_scoped","group":"weekly","percent":18,
             "scope":{"model":{"id":"claude-fable","display_name":"Duplicate"}}},
            {"kind":"weekly_scoped","group":"weekly","percent":19,
             "scope":{"model":{"id":"all-models","display_name":"All Models"}}},
            {"kind":"daily","group":"daily","percent":20,
             "scope":{"model":{"id":"ignored","display_name":"Ignored"}}}
        ],
        "extra_usage":{"is_enabled":true,"monthly_limit":5000,"used_credits":1250,
                       "utilization":25,"currency":"USD"}
    })";
    ClaudeCredentials credentials;
    credentials.rateLimitTier = QStringLiteral("default_claude_max_20x");
    credentials.subscriptionType = QStringLiteral("max");
    const QDateTime now =
        QDateTime::fromString(QStringLiteral("2026-09-04T04:00:00Z"), Qt::ISODate);
    QString error;

    const auto provider = ClaudeUsageParser::parse(payload, credentials, now, &error);

    QVERIFY2(provider.has_value(), qPrintable(error));
    QCOMPARE(provider->value(QStringLiteral("id")).toString(), QStringLiteral("claude"));
    QCOMPARE(provider->value(QStringLiteral("name")).toString(), QStringLiteral("Claude"));
    QCOMPARE(provider->value(QStringLiteral("source")).toString(), QStringLiteral("oauth"));
    QCOMPARE(provider->value(QStringLiteral("updatedAt")).toString(),
             QStringLiteral("2026-09-04T04:00:00.000Z"));
    QCOMPARE(provider->value(QStringLiteral("identity"))
                 .toMap()
                 .value(QStringLiteral("plan"))
                 .toString(),
             QStringLiteral("Claude Max 20x"));

    const QVariantList windows = provider->value(QStringLiteral("windows")).toList();
    QCOMPARE(windows.size(), 6);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("session"));
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("usedPercent")).toDouble(), 28.5);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("windowSeconds")).toInt(), 18'000);
    QCOMPARE(windows.at(1).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("weekly"));
    QCOMPARE(windows.at(2).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("model-sonnet-weekly"));
    QCOMPARE(windows.at(3).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("model-opus-weekly"));
    QCOMPARE(windows.at(4).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("model-fable-weekly"));
    QCOMPARE(windows.at(4).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Fable only"));
    QCOMPARE(windows.at(5).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("claude-routines"));

    const QVariantMap cost = provider->value(QStringLiteral("cost")).toMap();
    QCOMPARE(cost.value(QStringLiteral("usedUSD")).toDouble(), 12.5);
    QCOMPARE(cost.value(QStringLiteral("limitUSD")).toDouble(), 50.0);
    QCOMPARE(cost.value(QStringLiteral("period")).toString(), QStringLiteral("Monthly cap"));
}

void ClaudeUsageParserTest::mapsSpendLimitWithoutSession()
{
    const QByteArray payload = R"({
        "extra_usage":{"is_enabled":true,"monthly_limit":"2000","used_credits":"500",
                       "utilization":25,"currency":" usd "}
    })";
    ClaudeCredentials credentials;
    credentials.subscriptionType = QStringLiteral("enterprise");
    QString error;

    const auto provider = ClaudeUsageParser::parse(
        payload, credentials, QDateTime::fromSecsSinceEpoch(1'700'000'000, QTimeZone::UTC), &error);

    QVERIFY2(provider.has_value(), qPrintable(error));
    const QVariantList windows = provider->value(QStringLiteral("windows")).toList();
    QCOMPARE(windows.size(), 1);
    QCOMPARE(windows.first().toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("spend-limit"));
    QCOMPARE(windows.first().toMap().value(QStringLiteral("usedPercent")).toDouble(), 25.0);
    QCOMPARE(
        provider->value(QStringLiteral("cost")).toMap().value(QStringLiteral("period")).toString(),
        QStringLiteral("Spend limit"));
}

void ClaudeUsageParserTest::toleratesOptionalAndMalformedWindows()
{
    const QByteArray payload = R"({
        "five_hour":{"utilization":140,"resets_at":"bad"},
        "seven_day":{"utilization":"bad"},
        "seven_day_oauth_apps":{"utilization":-5},
        "routines":{"utilization":2},
        "limits":[4,{"kind":"weekly_scoped","group":"weekly","percent":2,
                     "scope":{"model":{"display_name":"---"}}}],
        "extra_usage":{"is_enabled":false,"monthly_limit":100,"used_credits":20}
    })";
    ClaudeCredentials credentials;
    credentials.rateLimitTier = QStringLiteral("pro");
    QString error;

    const auto provider =
        ClaudeUsageParser::parse(payload, credentials, QDateTime::currentDateTimeUtc(), &error);

    QVERIFY2(provider.has_value(), qPrintable(error));
    const QVariantList windows = provider->value(QStringLiteral("windows")).toList();
    QCOMPARE(windows.size(), 3);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("usedPercent")).toDouble(), 100.0);
    QVERIFY(!windows.at(0).toMap().contains(QStringLiteral("resetAt")));
    QCOMPARE(windows.at(1).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("oauth-apps-weekly"));
    QCOMPARE(windows.at(1).toMap().value(QStringLiteral("usedPercent")).toDouble(), 0.0);
    QCOMPARE(windows.at(2).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("claude-routines"));
    QVERIFY(provider->value(QStringLiteral("cost")).isNull());
}

void ClaudeUsageParserTest::rejectsInvalidResponses()
{
    ClaudeCredentials credentials;
    QString error;
    const QDateTime now = QDateTime::currentDateTimeUtc();

    QVERIFY(!ClaudeUsageParser::parse("{", credentials, now, &error));
    QCOMPARE(error, QStringLiteral("Claude usage API returned invalid JSON"));
    QVERIFY(!ClaudeUsageParser::parse("[]", credentials, now, &error));
    QCOMPARE(error, QStringLiteral("Claude usage API returned an invalid object"));
    QVERIFY(!ClaudeUsageParser::parse("{}", credentials, now, &error));
    QCOMPARE(error, QStringLiteral("Claude usage API returned no usable limits"));
    QVERIFY(!ClaudeUsageParser::parse("{", credentials, now, nullptr));
}

void ClaudeUsageParserTest::formatsPlans_data()
{
    QTest::addColumn<QString>("subscription");
    QTest::addColumn<QString>("tier");
    QTest::addColumn<QString>("expected");

    QTest::newRow("max multiplier") << "max" << "default_claude_max_5x" << "Claude Max 5x";
    QTest::newRow("pro subscription") << "pro" << "unknown" << "Claude Pro";
    QTest::newRow("team tier") << "" << "team_rate_limit" << "Claude Team";
    QTest::newRow("enterprise") << "enterprise" << "" << "Claude Enterprise";
    QTest::newRow("ultra") << "ultra" << "" << "Claude Ultra";
    QTest::newRow("unknown") << "unknown" << "unknown" << "";
}

void ClaudeUsageParserTest::formatsPlans()
{
    QFETCH(QString, subscription);
    QFETCH(QString, tier);
    QFETCH(QString, expected);
    QCOMPARE(ClaudeUsageParser::formatPlan(subscription, tier), expected);
}

QTEST_GUILESS_MAIN(ClaudeUsageParserTest)

#include "tst_claude_usage_parser.moc"
