#include <kodometer/codex_usage_parser.hpp>

#include <QTimeZone>
#include <QtTest>

using Kodometer::CodexCredentials;
using Kodometer::CodexUsageParser;

class CodexUsageParserTest final : public QObject
{
    Q_OBJECT

  private slots:
    void mapsUsageResponse();
    void toleratesOptionalAndMalformedWindows();
    void rejectsInvalidResponses();
    void formatsPlanNames_data();
    void formatsPlanNames();
};

void CodexUsageParserTest::mapsUsageResponse()
{
    const QByteArray payload = R"({
        "account_id": "response-account",
        "plan_type": "free_workspace",
        "rate_limit": {
            "primary_window": {
                "used_percent": 28,
                "reset_at": 1800000000,
                "limit_window_seconds": 18000
            },
            "secondary_window": {
                "used_percent": 41,
                "reset_at": 1800100000,
                "limit_window_seconds": 604800
            }
        },
        "additional_rate_limits": [{
            "limit_name": "GPT-5.3-Codex-Spark",
            "metered_feature": "spark",
            "rate_limit": {
                "primary_window": {
                    "used_percent": 9,
                    "reset_at": 1800200000,
                    "limit_window_seconds": 3600
                }
            }
        }],
        "credits": {
            "has_credits": true,
            "unlimited": false,
            "balance": "112.45"
        }
    })";
    CodexCredentials credentials;
    credentials.accountId = QStringLiteral("credential-account");
    credentials.email = QStringLiteral("person@example.com");
    const QDateTime now =
        QDateTime::fromString(QStringLiteral("2026-09-04T04:00:00Z"), Qt::ISODate);
    QString error;

    const auto provider = CodexUsageParser::parse(payload, credentials, now, &error);

    QVERIFY2(provider.has_value(), qPrintable(error));
    QCOMPARE(provider->value(QStringLiteral("id")).toString(), QStringLiteral("codex"));
    QCOMPARE(provider->value(QStringLiteral("name")).toString(), QStringLiteral("Codex"));
    QCOMPARE(provider->value(QStringLiteral("source")).toString(), QStringLiteral("oauth"));
    QCOMPARE(provider->value(QStringLiteral("updatedAt")).toString(),
             QStringLiteral("2026-09-04T04:00:00.000Z"));

    const QVariantMap identity = provider->value(QStringLiteral("identity")).toMap();
    QCOMPARE(identity.value(QStringLiteral("accountEmail")).toString(),
             QStringLiteral("redacted@example.com"));
    QCOMPARE(identity.value(QStringLiteral("plan")).toString(), QStringLiteral("Free Workspace"));
    QCOMPARE(identity.value(QStringLiteral("accountId")).toString(),
             QStringLiteral("response-account"));

    const QVariantList windows = provider->value(QStringLiteral("windows")).toList();
    QCOMPARE(windows.size(), 3);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("session"));
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("usedPercent")).toDouble(), 28.0);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("remainingPercent")).toDouble(), 72.0);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("resetAt")).toString(),
             QStringLiteral("2027-01-15T08:00:00.000Z"));
    QCOMPARE(windows.at(1).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("weekly"));
    QCOMPARE(windows.at(2).toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("model-spark"));
    QCOMPARE(windows.at(2).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("GPT-5.3-Codex-Spark"));

    const QVariantMap credits = provider->value(QStringLiteral("credits")).toMap();
    QCOMPARE(credits.value(QStringLiteral("remaining")).toDouble(), 112.45);
    QCOMPARE(credits.value(QStringLiteral("unit")).toString(), QStringLiteral("credits"));
}

void CodexUsageParserTest::toleratesOptionalAndMalformedWindows()
{
    const QByteArray payload = R"({
        "plan_type": "future_plan",
        "rate_limit": {
            "primary_window": {"used_percent": "bad"},
            "secondary_window": null
        },
        "additional_rate_limits": [
            {"limit_name":"Broken","rate_limit":{"primary_window":{"used_percent":2}}},
            4,
            {"metered_feature":"fast","rate_limit":null}
        ],
        "credits": {"has_credits": true, "unlimited": true}
    })";
    CodexCredentials credentials;
    credentials.apiKey = true;
    credentials.accountId = QStringLiteral("fallback-account");
    const QDateTime now = QDateTime::fromSecsSinceEpoch(1'700'000'000, QTimeZone::UTC);
    QString error;

    const auto provider = CodexUsageParser::parse(payload, credentials, now, &error);

    QVERIFY2(provider.has_value(), qPrintable(error));
    QCOMPARE(provider->value(QStringLiteral("source")).toString(), QStringLiteral("api-key"));
    QVERIFY(provider->value(QStringLiteral("windows")).toList().isEmpty());
    QVERIFY(provider->value(QStringLiteral("credits")).isNull());
    const QVariantMap identity = provider->value(QStringLiteral("identity")).toMap();
    QCOMPARE(identity.value(QStringLiteral("plan")).toString(), QStringLiteral("Future Plan"));
    QCOMPARE(identity.value(QStringLiteral("accountId")).toString(),
             QStringLiteral("fallback-account"));
    QVERIFY(!identity.contains(QStringLiteral("accountEmail")));
}

void CodexUsageParserTest::rejectsInvalidResponses()
{
    CodexCredentials credentials;
    QString error;

    QVERIFY(!CodexUsageParser::parse("{", credentials, QDateTime::currentDateTimeUtc(), &error));
    QCOMPARE(error, QStringLiteral("Codex usage API returned invalid JSON"));

    QVERIFY(!CodexUsageParser::parse("[]", credentials, QDateTime::currentDateTimeUtc(), &error));
    QCOMPARE(error, QStringLiteral("Codex usage API returned an invalid object"));
}

void CodexUsageParserTest::formatsPlanNames_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plus") << QStringLiteral("plus") << QStringLiteral("Plus");
    QTest::newRow("free workspace")
        << QStringLiteral("free_workspace") << QStringLiteral("Free Workspace");
    QTest::newRow("hyphen") << QStringLiteral("team-plan") << QStringLiteral("Team Plan");
    QTest::newRow("empty") << QString() << QString();
}

void CodexUsageParserTest::formatsPlanNames()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(CodexUsageParser::formatPlan(input), expected);
}

QTEST_GUILESS_MAIN(CodexUsageParserTest)

#include "tst_codex_usage_parser.moc"
