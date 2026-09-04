#include <kodometer/xai_usage_parser.hpp>

#include <QTimeZone>
#include <QtTest>

#include <limits>

using Kodometer::XaiBalance;
using Kodometer::XaiUsageHistory;
using Kodometer::XaiUsageParser;

class XaiUsageParserTest final : public QObject
{
    Q_OBJECT

  private slots:
    void parsesInvertedLedgerBalances_data();
    void parsesInvertedLedgerBalances();
    void rejectsMalformedBalances();
    void aggregatesDailySpendHistory();
    void acceptsEmptyHistoryAndRejectsMalformedHistory();
    void mapsProviderWithAndWithoutHistory();
};

void XaiUsageParserTest::parsesInvertedLedgerBalances_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<double>("balance");

    QTest::newRow("positive-ledger") << QByteArray(R"({"total":{"val":"2500"}})") << -25.0;
    QTest::newRow("zero") << QByteArray(R"({"total":{"val":"0"}})") << 0.0;
    QTest::newRow("credit") << QByteArray(R"({"total":{"val":"-333"}})") << 3.33;
    QTest::newRow("fractional-cent") << QByteArray(R"({"total":{"val":"-12.5"}})") << 0.125;
}

void XaiUsageParserTest::parsesInvertedLedgerBalances()
{
    QFETCH(QByteArray, body);
    QFETCH(double, balance);
    QString error;

    const auto parsed = XaiUsageParser::parseBalance(body, &error);

    QVERIFY2(parsed.has_value(), qPrintable(error));
    QCOMPARE(parsed->balanceUsd, balance);
}

void XaiUsageParserTest::rejectsMalformedBalances()
{
    QString error;
    QVERIFY(!XaiUsageParser::parseBalance("{", &error));
    QCOMPARE(error, QStringLiteral("xAI balance API returned invalid JSON"));
    QVERIFY(!XaiUsageParser::parseBalance("[]", &error));
    QCOMPARE(error, QStringLiteral("xAI balance API returned an invalid object"));

    for (const QByteArray &body :
         {QByteArray("{}"), QByteArray(R"({"total":{"val":7}})"),
          QByteArray(R"({"total":{"val":"n/a"}})"), QByteArray(R"({"total":{"val":"1e999"}})")}) {
        QVERIFY(!XaiUsageParser::parseBalance(body, &error));
        QCOMPARE(error, QStringLiteral("xAI balance API did not return a valid cent amount"));
    }
    QVERIFY(!XaiUsageParser::parseBalance("{", nullptr));
}

void XaiUsageParserTest::aggregatesDailySpendHistory()
{
    QString error;
    const auto history = XaiUsageParser::parseHistory(
        R"({
          "timeSeries":[
            {"dataPoints":[
              {"timestamp":"2027-01-13T00:00:00Z","values":[0.75]},
              {"timestamp":"2027-01-14T10:00:00Z","values":[0.5]},
              {"timestamp":"2027-01-15T00:00:00Z","values":[0]}
            ]},
            {"dataPoints":[
              {"timestamp":"2027-01-13T12:00:00Z","values":[0.5]},
              {"timestamp":"2027-01-14T00:00:00Z","values":[0.01]}
            ]}
          ],
          "limitReached":true
        })",
        &error);

    QVERIFY2(history.has_value(), qPrintable(error));
    QCOMPARE(history->daily.size(), 3);
    QCOMPARE(history->daily.value(QStringLiteral("2027-01-13")), 1.25);
    QCOMPARE(history->daily.value(QStringLiteral("2027-01-14")), 0.51);
    QCOMPARE(history->daily.value(QStringLiteral("2027-01-15")), 0.0);
    QVERIFY(history->partial);
}

void XaiUsageParserTest::acceptsEmptyHistoryAndRejectsMalformedHistory()
{
    QString error;
    const auto empty =
        XaiUsageParser::parseHistory(R"({"timeSeries":[],"limitReached":false})", &error);
    QVERIFY2(empty.has_value(), qPrintable(error));
    QVERIFY(empty->daily.isEmpty());
    QVERIFY(!empty->partial);

    const QList<QByteArray> malformed{
        QByteArray("{"),
        QByteArray("[]"),
        QByteArray("{}"),
        QByteArray(R"({"timeSeries":["invalid"]})"),
        QByteArray(R"({"timeSeries":[{}]})"),
        QByteArray(R"({"timeSeries":[{"dataPoints":["invalid"]}]})"),
        QByteArray(R"({"timeSeries":[{"dataPoints":[{}]}]})"),
        QByteArray(R"({"timeSeries":[{"dataPoints":[{"timestamp":"bad","values":[1]}]}]})"),
        QByteArray(
            R"({"timeSeries":[{"dataPoints":[{"timestamp":"2027-01-15T00:00:00Z","values":[]}]}]})"),
        QByteArray(
            R"({"timeSeries":[{"dataPoints":[{"timestamp":"2027-01-15T00:00:00Z","values":[-1]}]}]})"),
    };
    for (const QByteArray &body : malformed) {
        QVERIFY(!XaiUsageParser::parseHistory(body, &error));
        QVERIFY(error.startsWith(QStringLiteral("xAI usage API")));
    }
}

void XaiUsageParserTest::mapsProviderWithAndWithoutHistory()
{
    const XaiBalance balance{10.0};
    XaiUsageHistory history;
    history.daily = {{QStringLiteral("2027-01-13"), 0.5},
                     {QStringLiteral("2027-01-14"), 0.01},
                     {QStringLiteral("2027-01-15"), 1.25}};
    history.partial = true;
    const QDateTime updatedAt = QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC);

    const QVariantMap provider = XaiUsageParser::provider(balance, history, updatedAt);

    QCOMPARE(provider.value(QStringLiteral("id")).toString(), QStringLiteral("xai"));
    QCOMPARE(provider.value(QStringLiteral("name")).toString(), QStringLiteral("xAI"));
    QCOMPARE(provider.value(QStringLiteral("source")).toString(), QStringLiteral("api-key"));
    QVERIFY(provider.value(QStringLiteral("windows")).toList().isEmpty());
    QCOMPARE(
        provider.value(QStringLiteral("identity")).toMap().value(QStringLiteral("plan")).toString(),
        QStringLiteral("Management API"));
    const QVariantMap cost = provider.value(QStringLiteral("cost")).toMap();
    QCOMPARE(cost.value(QStringLiteral("balanceUSD")).toDouble(), 10.0);
    QCOMPARE(cost.value(QStringLiteral("todayUSD")).toDouble(), 1.25);
    QCOMPARE(cost.value(QStringLiteral("last30DaysUSD")).toDouble(), 1.76);
    QCOMPARE(cost.value(QStringLiteral("period")).toString(), QStringLiteral("Prepaid credits"));
    QVERIFY(cost.value(QStringLiteral("historyPartial")).toBool());
    QCOMPARE(cost.value(QStringLiteral("daily")).toList().size(), 3);
    QCOMPARE(provider.value(QStringLiteral("dataConfidence")).toString(),
             QStringLiteral("estimated"));
    QCOMPARE(provider.value(QStringLiteral("updatedAt")).toString(),
             QStringLiteral("2027-01-15T08:00:00.000Z"));

    const QVariantMap balanceOnly = XaiUsageParser::provider(balance, std::nullopt, updatedAt);
    const QVariantMap balanceCost = balanceOnly.value(QStringLiteral("cost")).toMap();
    QCOMPARE(balanceCost.value(QStringLiteral("balanceUSD")).toDouble(), 10.0);
    QVERIFY(!balanceCost.contains(QStringLiteral("todayUSD")));
    QVERIFY(!balanceCost.contains(QStringLiteral("last30DaysUSD")));
    QVERIFY(!balanceCost.contains(QStringLiteral("daily")));
    QCOMPARE(balanceOnly.value(QStringLiteral("dataConfidence")).toString(),
             QStringLiteral("exact"));
}

QTEST_GUILESS_MAIN(XaiUsageParserTest)

#include "tst_xai_usage_parser.moc"
