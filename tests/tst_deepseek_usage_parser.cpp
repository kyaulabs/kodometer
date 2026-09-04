#include <kodometer/deepseek_usage_parser.hpp>

#include <QDateTime>
#include <QTest>
#include <QTimeZone>
#include <QVariantList>
#include <QVariantMap>

using namespace Kodometer;

class DeepSeekUsageParserTest final : public QObject
{
    Q_OBJECT

  private slots:
    void parsesUsdBalance();
    void selectsFundedCurrency();
    void mapsUnavailableAndEmptyBalances();
    void acceptsNumericAmounts();
    void rejectsMalformedResponses();
};

void DeepSeekUsageParserTest::parsesUsdBalance()
{
    const QByteArray payload = R"({
      "is_available": true,
      "balance_infos": [{
        "currency": "USD",
        "total_balance": "50.00",
        "granted_balance": "10.00",
        "topped_up_balance": "40.00"
      }]
    })";
    const QDateTime now = QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC);

    QString error;
    const auto provider = DeepSeekUsageParser::parse(payload, now, &error);
    QVERIFY(provider.has_value());
    QVERIFY(error.isEmpty());
    QCOMPARE(provider->value(QStringLiteral("id")), QStringLiteral("deepseek"));
    QCOMPARE(provider->value(QStringLiteral("name")), QStringLiteral("DeepSeek"));
    QCOMPARE(provider->value(QStringLiteral("source")), QStringLiteral("api-key"));
    QCOMPARE(provider->value(QStringLiteral("windows")).toList().size(), 0);
    QCOMPARE(provider->value(QStringLiteral("updatedAt")),
             QStringLiteral("2027-01-15T08:00:00.000Z"));
    QCOMPARE(provider->value(QStringLiteral("identity")).toMap().value(QStringLiteral("plan")),
             QStringLiteral("API credits"));
    QVERIFY(!provider->value(QStringLiteral("status")).isValid());

    const QVariantMap cost = provider->value(QStringLiteral("cost")).toMap();
    QCOMPARE(cost.value(QStringLiteral("balance")).toDouble(), 50.0);
    QCOMPARE(cost.value(QStringLiteral("balanceUSD")).toDouble(), 50.0);
    QCOMPARE(cost.value(QStringLiteral("grantedBalance")).toDouble(), 10.0);
    QCOMPARE(cost.value(QStringLiteral("toppedUpBalance")).toDouble(), 40.0);
    QCOMPARE(cost.value(QStringLiteral("currencyCode")), QStringLiteral("USD"));
    QCOMPARE(cost.value(QStringLiteral("period")), QStringLiteral("Account balance"));
    QCOMPARE(cost.value(QStringLiteral("available")).toBool(), true);
}

void DeepSeekUsageParserTest::selectsFundedCurrency()
{
    const auto usd = DeepSeekUsageParser::parse(
        R"({"is_available":true,"balance_infos":[
          {"currency":"CNY","total_balance":"100","granted_balance":"10","topped_up_balance":"90"},
          {"currency":"USD","total_balance":"20","granted_balance":"5","topped_up_balance":"15"}]})",
        QDateTime::currentDateTimeUtc());
    QVERIFY(usd.has_value());
    QCOMPARE(usd->value(QStringLiteral("cost")).toMap().value(QStringLiteral("currencyCode")),
             QStringLiteral("USD"));

    const auto cny = DeepSeekUsageParser::parse(
        R"({"is_available":true,"balance_infos":[
          {"currency":"USD","total_balance":"0","granted_balance":"0","topped_up_balance":"0"},
          {"currency":"CNY","total_balance":"100","granted_balance":"0","topped_up_balance":"100"}]})",
        QDateTime::currentDateTimeUtc());
    QVERIFY(cny.has_value());
    const QVariantMap cost = cny->value(QStringLiteral("cost")).toMap();
    QCOMPARE(cost.value(QStringLiteral("currencyCode")), QStringLiteral("CNY"));
    QCOMPARE(cost.value(QStringLiteral("balance")).toDouble(), 100.0);
    QVERIFY(!cost.contains(QStringLiteral("balanceUSD")));
}

void DeepSeekUsageParserTest::mapsUnavailableAndEmptyBalances()
{
    const auto unavailable = DeepSeekUsageParser::parse(
        R"({"is_available":false,"balance_infos":[
          {"currency":"USD","total_balance":"5","granted_balance":"0","topped_up_balance":"5"}]})",
        QDateTime::currentDateTimeUtc());
    QVERIFY(unavailable.has_value());
    QVariantMap status = unavailable->value(QStringLiteral("status")).toMap();
    QCOMPARE(status.value(QStringLiteral("label")), QStringLiteral("Unavailable for API calls"));
    QCOMPARE(status.value(QStringLiteral("level")), QStringLiteral("warning"));

    const auto empty = DeepSeekUsageParser::parse(R"({"is_available":true,"balance_infos":[]})",
                                                  QDateTime::currentDateTimeUtc());
    QVERIFY(empty.has_value());
    const QVariantMap cost = empty->value(QStringLiteral("cost")).toMap();
    QCOMPARE(cost.value(QStringLiteral("balance")).toDouble(), 0.0);
    QCOMPARE(cost.value(QStringLiteral("currencyCode")), QStringLiteral("USD"));
    status = empty->value(QStringLiteral("status")).toMap();
    QCOMPARE(status.value(QStringLiteral("label")), QStringLiteral("No credits"));
    QCOMPARE(status.value(QStringLiteral("level")), QStringLiteral("critical"));

    const auto zero = DeepSeekUsageParser::parse(
        R"({"is_available":false,"balance_infos":[
          {"currency":"CNY","total_balance":"0","granted_balance":"0","topped_up_balance":"0"}]})",
        QDateTime::currentDateTimeUtc());
    QVERIFY(zero.has_value());
    QCOMPARE(zero->value(QStringLiteral("status")).toMap().value(QStringLiteral("label")),
             QStringLiteral("No credits"));
}

void DeepSeekUsageParserTest::acceptsNumericAmounts()
{
    const auto provider = DeepSeekUsageParser::parse(
        R"({"is_available":true,"balance_infos":[{
          "currency":"usd","total_balance":8.47,"granted_balance":0.5,"topped_up_balance":7.97
        }]})",
        QDateTime::currentDateTimeUtc());
    QVERIFY(provider.has_value());
    const QVariantMap cost = provider->value(QStringLiteral("cost")).toMap();
    QCOMPARE(cost.value(QStringLiteral("currencyCode")), QStringLiteral("USD"));
    QCOMPARE(cost.value(QStringLiteral("balance")).toDouble(), 8.47);
    QCOMPARE(cost.value(QStringLiteral("grantedBalance")).toDouble(), 0.5);
    QCOMPARE(cost.value(QStringLiteral("toppedUpBalance")).toDouble(), 7.97);
}

void DeepSeekUsageParserTest::rejectsMalformedResponses()
{
    const QList<QPair<QByteArray, QString>> cases{
        {"{", QStringLiteral("DeepSeek balance API returned invalid JSON")},
        {"[]", QStringLiteral("DeepSeek balance API returned an invalid object")},
        {R"({"balance_infos":[]})", QStringLiteral("DeepSeek balance API omitted availability")},
        {R"({"is_available":true})",
         QStringLiteral("DeepSeek balance API omitted balance information")},
        {R"({"is_available":true,"balance_infos":["invalid"]})",
         QStringLiteral("DeepSeek balance API returned malformed balance information")},
        {R"({"is_available":true,"balance_infos":[{
          "currency":"USD","total_balance":"bad","granted_balance":"0","topped_up_balance":"0"}]})",
         QStringLiteral("DeepSeek balance API returned a non-numeric balance")},
        {R"({"is_available":true,"balance_infos":[{
          "currency":"","total_balance":"1","granted_balance":"0","topped_up_balance":"1"}]})",
         QStringLiteral("DeepSeek balance API returned an invalid currency")},
        {R"({"is_available":true,"balance_infos":[{
          "currency":"USD","total_balance":"NaN","granted_balance":"0","topped_up_balance":"0"}]})",
         QStringLiteral("DeepSeek balance API returned a non-numeric balance")},
    };

    for (const auto &[payload, expected] : cases) {
        QString error;
        QVERIFY(!DeepSeekUsageParser::parse(payload, QDateTime::currentDateTimeUtc(), &error)
                     .has_value());
        QCOMPARE(error, expected);
    }
}

QTEST_GUILESS_MAIN(DeepSeekUsageParserTest)
#include "tst_deepseek_usage_parser.moc"
