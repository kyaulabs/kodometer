#include <kodometer/kimi_usage_parser.hpp>

#include <QDateTime>
#include <QTest>
#include <QTimeZone>
#include <QVariantList>
#include <QVariantMap>

using namespace Kodometer;

class KimiUsageParserTest final : public QObject
{
    Q_OBJECT

  private slots:
    void parsesOfficialUsageResponse();
    void acceptsNumericCountersAndResetAliases();
    void derivesAndBoundsCounters();
    void handlesOptionalRateLimitMetadata();
    void rejectsMalformedUsage();
};

void KimiUsageParserTest::parsesOfficialUsageResponse()
{
    const QByteArray payload = R"({
      "usage": {
        "limit": "2048",
        "used": "375",
        "remaining": "1673",
        "resetTime": "2027-01-22T08:00:00.123456789Z"
      },
      "limits": [{
        "window": {"duration": 300, "timeUnit": "TIME_UNIT_MINUTE"},
        "detail": {
          "limit": "200",
          "used": "19",
          "remaining": "181",
          "resetTime": "2027-01-15T13:00:00Z"
        }
      }]
    })";
    const QDateTime now = QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC);

    QString error;
    const auto provider =
        KimiUsageParser::parse(payload, KimiCredentialSource::ApiKey, now, &error);
    QVERIFY(provider.has_value());
    QVERIFY(error.isEmpty());
    QCOMPARE(provider->value(QStringLiteral("id")).toString(), QStringLiteral("kimi"));
    QCOMPARE(provider->value(QStringLiteral("name")).toString(), QStringLiteral("Kimi Code"));
    QCOMPARE(provider->value(QStringLiteral("source")).toString(), QStringLiteral("api-key"));
    QCOMPARE(provider->value(QStringLiteral("updatedAt")).toString(),
             QStringLiteral("2027-01-15T08:00:00.000Z"));
    QCOMPARE(provider->value(QStringLiteral("identity")).toMap().value(QStringLiteral("plan")),
             QStringLiteral("Moderato"));

    const QVariantList windows = provider->value(QStringLiteral("windows")).toList();
    QCOMPARE(windows.size(), 2);
    const QVariantMap weekly = windows.at(0).toMap();
    QCOMPARE(weekly.value(QStringLiteral("kind")), QStringLiteral("weekly"));
    QCOMPARE(weekly.value(QStringLiteral("label")), QStringLiteral("7-day usage"));
    QCOMPARE(weekly.value(QStringLiteral("windowSeconds")), 7 * 24 * 60 * 60);
    QVERIFY(qAbs(weekly.value(QStringLiteral("usedPercent")).toDouble() -
                 (375.0 / 2048.0 * 100.0)) < 0.001);
    QCOMPARE(weekly.value(QStringLiteral("resetAt")), QStringLiteral("2027-01-22T08:00:00.123Z"));

    const QVariantMap session = windows.at(1).toMap();
    QCOMPARE(session.value(QStringLiteral("kind")), QStringLiteral("session"));
    QCOMPARE(session.value(QStringLiteral("label")), QStringLiteral("5-hour usage"));
    QCOMPARE(session.value(QStringLiteral("windowSeconds")), 5 * 60 * 60);
    QCOMPARE(session.value(QStringLiteral("usedPercent")).toDouble(), 9.5);
    QCOMPARE(session.value(QStringLiteral("remainingPercent")).toDouble(), 90.5);
}

void KimiUsageParserTest::acceptsNumericCountersAndResetAliases()
{
    const QByteArray payload = R"({
      "usage": {"limit": 1000, "used": 40, "remaining": 960,
                "reset_at": "2027-01-22T08:00:00Z"},
      "limits": [{
        "window": {"duration": 2, "timeUnit": "TIME_UNIT_HOUR"},
        "detail": {"limit": 100, "remaining": 99,
                   "resetAt": "2027-01-15T10:00:00Z"}
      }]
    })";

    const auto provider =
        KimiUsageParser::parse(payload, KimiCredentialSource::Cli,
                               QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC));
    QVERIFY(provider.has_value());
    QCOMPARE(provider->value(QStringLiteral("source")), QStringLiteral("cli-oauth"));
    const QVariantList windows = provider->value(QStringLiteral("windows")).toList();
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("usedPercent")).toDouble(), 4.0);
    QCOMPARE(windows.at(0).toMap().value(QStringLiteral("resetAt")),
             QStringLiteral("2027-01-22T08:00:00.000Z"));
    QCOMPARE(windows.at(1).toMap().value(QStringLiteral("usedPercent")).toDouble(), 1.0);
    QCOMPARE(windows.at(1).toMap().value(QStringLiteral("windowSeconds")), 2 * 60 * 60);
    QCOMPARE(windows.at(1).toMap().value(QStringLiteral("label")), QStringLiteral("2-hour usage"));
}

void KimiUsageParserTest::derivesAndBoundsCounters()
{
    const QList<QPair<QByteArray, double>> cases{
        {R"({"usage":{"limit":"100","used":"125","remaining":"25"}})", 100.0},
        {R"({"usage":{"limit":"100","used":"invalid","remaining":"75"}})", 25.0},
        {R"({"usage":{"limit":"100","used":"-1","remaining":"75"}})", 25.0},
        {R"({"usage":{"limit":"100"}})", 0.0},
        {R"({"usage":{"limit":"100","remaining":"101"}})", 0.0},
    };

    for (const auto &[payload, expected] : cases) {
        const auto provider = KimiUsageParser::parse(payload, KimiCredentialSource::ApiKey,
                                                     QDateTime::currentDateTimeUtc());
        QVERIFY(provider.has_value());
        const QVariantMap window =
            provider->value(QStringLiteral("windows")).toList().first().toMap();
        QCOMPARE(window.value(QStringLiteral("usedPercent")).toDouble(), expected);
        if (payload.contains("\"used\":\"125\"") || payload.contains("\"remaining\":\"75\"")) {
            QVERIFY(window.contains(QStringLiteral("windowSeconds")));
        }
        else {
            QVERIFY(!window.contains(QStringLiteral("windowSeconds")));
        }
    }

    const auto andante =
        KimiUsageParser::parse(R"({"usage":{"limit":"1024","used":"0"}})",
                               KimiCredentialSource::ApiKey, QDateTime::currentDateTimeUtc());
    const auto allegretto =
        KimiUsageParser::parse(R"({"usage":{"limit":"7168","used":"0"}})",
                               KimiCredentialSource::ApiKey, QDateTime::currentDateTimeUtc());
    QVERIFY(andante.has_value());
    QVERIFY(allegretto.has_value());
    QCOMPARE(andante->value(QStringLiteral("identity")).toMap().value(QStringLiteral("plan")),
             QStringLiteral("Andante"));
    QCOMPARE(allegretto->value(QStringLiteral("identity")).toMap().value(QStringLiteral("plan")),
             QStringLiteral("Allegretto"));
}

void KimiUsageParserTest::handlesOptionalRateLimitMetadata()
{
    const auto missing =
        KimiUsageParser::parse(R"({"usage":{"limit":"100","used":"25"},"limits":null})",
                               KimiCredentialSource::ApiKey, QDateTime::currentDateTimeUtc());
    QVERIFY(missing.has_value());
    QCOMPARE(missing->value(QStringLiteral("windows")).toList().size(), 1);

    const auto malformed =
        KimiUsageParser::parse(R"({"usage":{"limit":"100","used":"25"},"limits":[{"detail":{}}]})",
                               KimiCredentialSource::ApiKey, QDateTime::currentDateTimeUtc());
    QVERIFY(malformed.has_value());
    QCOMPARE(malformed->value(QStringLiteral("windows")).toList().size(), 1);

    const auto unknownUnit = KimiUsageParser::parse(
        R"({"usage":{"limit":"100","used":"25"},"limits":[{
          "window":{"duration":5,"timeUnit":"TIME_UNIT_UNKNOWN"},
          "detail":{"limit":"20","used":"5"}}]})",
        KimiCredentialSource::ApiKey, QDateTime::currentDateTimeUtc());
    QVERIFY(unknownUnit.has_value());
    const QVariantMap rate = unknownUnit->value(QStringLiteral("windows")).toList().at(1).toMap();
    QVERIFY(!rate.contains(QStringLiteral("windowSeconds")));
    QCOMPARE(rate.value(QStringLiteral("label")), QStringLiteral("Rate limit"));

    const auto day = KimiUsageParser::parse(
        R"({"usage":{"limit":"100","used":"25"},"limits":[{
          "window":{"duration":1,"timeUnit":"TIME_UNIT_DAY"},
          "detail":{"limit":"20","used":"5"}}]})",
        KimiCredentialSource::ApiKey, QDateTime::currentDateTimeUtc());
    QVERIFY(day.has_value());
    QCOMPARE(day->value(QStringLiteral("windows"))
                 .toList()
                 .at(1)
                 .toMap()
                 .value(QStringLiteral("windowSeconds")),
             24 * 60 * 60);
}

void KimiUsageParserTest::rejectsMalformedUsage()
{
    const QList<QPair<QByteArray, QString>> cases{
        {"{", QStringLiteral("Kimi Code usage API returned invalid JSON")},
        {"[]", QStringLiteral("Kimi Code usage API returned an invalid object")},
        {"{}", QStringLiteral("Kimi Code usage API returned no usage detail")},
        {R"({"usage":[]})", QStringLiteral("Kimi Code usage API returned no usage detail")},
        {R"({"usage":{"used":"1"}})",
         QStringLiteral("Kimi Code usage API returned an invalid weekly limit")},
        {R"({"usage":{"limit":"0","used":"0"}})",
         QStringLiteral("Kimi Code usage API returned an invalid weekly limit")},
        {R"({"usage":{"limit":"invalid","used":"0"}})",
         QStringLiteral("Kimi Code usage API returned an invalid weekly limit")},
    };

    for (const auto &[payload, expected] : cases) {
        QString error;
        QVERIFY(!KimiUsageParser::parse(payload, KimiCredentialSource::ApiKey,
                                        QDateTime::currentDateTimeUtc(), &error)
                     .has_value());
        QCOMPARE(error, expected);
    }
}

QTEST_GUILESS_MAIN(KimiUsageParserTest)
#include "tst_kimi_usage_parser.moc"
