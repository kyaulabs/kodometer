#include <kodometer/zai_usage_parser.hpp>

#include <QDateTime>
#include <QTest>
#include <QTimeZone>
#include <QVariantList>
#include <QVariantMap>

using namespace Kodometer;

namespace {

QVariantMap windowByKind(const QVariantMap &provider, const QString &kind)
{
    for (const QVariant &value : provider.value(QStringLiteral("windows")).toList()) {
        const QVariantMap window = value.toMap();
        if (window.value(QStringLiteral("kind")).toString() == kind) {
            return window;
        }
    }
    return {};
}

} // namespace

class ZaiUsageParserTest final : public QObject
{
    Q_OBJECT

  private slots:
    void mapsQuotaWindowsAndDetails();
    void derivesPercentagesFromCounts();
    void mapsCreditQuotaRate();
    void handlesTimeOnlyAndUnknownLimits();
    void rejectsMalformedQuotaResponses();
    void parsesModelUsage();
    void rejectsMalformedModelUsage();
    void parsesChinaBalance();
};

void ZaiUsageParserTest::mapsQuotaWindowsAndDetails()
{
    const QByteArray payload = R"({"code":200,"msg":"success","success":true,"data":{
      "planName":"Pro","limits":[
        {"type":"TOKENS_LIMIT","unit":3,"number":5,"percentage":25,"nextResetTime":1785816000000},
        {"type":"TOKENS_LIMIT","unit":6,"number":1,"percentage":9,"nextResetTime":1786291200000},
        {"type":"TIME_LIMIT","unit":5,"number":1,"usage":1000,"currentValue":224,
         "remaining":776,"percentage":22,"usageDetails":[
           {"modelCode":"search-prime","usage":210},
           {"modelCode":"web-reader","usage":14}]}
      ]}})";
    const QDateTime now = QDateTime::fromSecsSinceEpoch(1'785'816'000, QTimeZone::UTC);

    QString error;
    const auto provider = ZaiUsageParser::parseQuota(payload, ZaiRegion::Global,
                                                     ZaiUsageScope::Personal, now, &error);
    QVERIFY(provider.has_value());
    QVERIFY(error.isEmpty());
    QCOMPARE(provider->value(QStringLiteral("id")), QStringLiteral("zai"));
    QCOMPARE(provider->value(QStringLiteral("name")), QStringLiteral("z.ai / GLM"));
    QCOMPARE(provider->value(QStringLiteral("source")), QStringLiteral("api-key"));
    QCOMPARE(provider->value(QStringLiteral("identity")).toMap().value(QStringLiteral("plan")),
             QStringLiteral("Pro"));
    QCOMPARE(provider->value(QStringLiteral("identity")).toMap().value(QStringLiteral("region")),
             QStringLiteral("Global"));
    QCOMPARE(provider->value(QStringLiteral("updatedAt")),
             QStringLiteral("2026-08-04T04:00:00.000Z"));

    const QVariantList windows = provider->value(QStringLiteral("windows")).toList();
    QCOMPARE(windows.size(), 3);
    const QVariantMap session = windowByKind(*provider, QStringLiteral("session"));
    QCOMPARE(session.value(QStringLiteral("label")), QStringLiteral("5-hour usage"));
    QCOMPARE(session.value(QStringLiteral("windowSeconds")).toInt(), 18'000);
    QCOMPARE(session.value(QStringLiteral("usedPercent")).toDouble(), 25.0);
    QCOMPARE(session.value(QStringLiteral("remainingPercent")).toDouble(), 75.0);
    QCOMPARE(session.value(QStringLiteral("resetAt")), QStringLiteral("2026-08-04T04:00:00.000Z"));

    const QVariantMap weekly = windowByKind(*provider, QStringLiteral("weekly"));
    QCOMPARE(weekly.value(QStringLiteral("label")), QStringLiteral("Weekly usage"));
    QCOMPARE(weekly.value(QStringLiteral("windowSeconds")).toInt(), 604'800);
    QCOMPARE(weekly.value(QStringLiteral("usedPercent")).toDouble(), 9.0);

    const QVariantMap mcp = windowByKind(*provider, QStringLiteral("mcp"));
    QCOMPARE(mcp.value(QStringLiteral("label")), QStringLiteral("MCP"));
    QCOMPARE(mcp.value(QStringLiteral("windowSeconds")).toInt(), 30 * 24 * 60 * 60);
    QCOMPARE(mcp.value(QStringLiteral("usedPercent")).toDouble(), 22.4);

    const QVariantList sections = provider->value(QStringLiteral("details")).toList();
    QCOMPARE(sections.size(), 1);
    const QVariantList rows = sections.first().toMap().value(QStringLiteral("rows")).toList();
    QCOMPARE(rows.size(), 5);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("label")), QStringLiteral("Token quota"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("label")),
             QStringLiteral("Session token quota"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("label")), QStringLiteral("MCP quota"));
    QCOMPARE(rows.at(3).toMap().value(QStringLiteral("label")), QStringLiteral("search-prime"));
    QCOMPARE(rows.at(3).toMap().value(QStringLiteral("value")), QStringLiteral("210"));
}

void ZaiUsageParserTest::derivesPercentagesFromCounts()
{
    const QByteArray payload = R"({"code":200,"success":true,"data":{"limits":[
      {"type":"CREDIT_LIMIT","unit":3,"number":5,"usage":100,"currentValue":80,
       "remaining":40,"percentage":1},
      {"type":"TOKENS_LIMIT","unit":6,"number":1,"usage":10,"remaining":20,
       "percentage":200}
    ]}})";
    const auto provider =
        ZaiUsageParser::parseQuota(payload, ZaiRegion::BigModelChina, ZaiUsageScope::Team,
                                   QDateTime::fromSecsSinceEpoch(1'785'826'800, QTimeZone::UTC));
    QVERIFY(provider.has_value());
    QCOMPARE(windowByKind(*provider, QStringLiteral("session"))
                 .value(QStringLiteral("usedPercent"))
                 .toDouble(),
             80.0);
    QCOMPARE(windowByKind(*provider, QStringLiteral("weekly"))
                 .value(QStringLiteral("usedPercent"))
                 .toDouble(),
             0.0);
    QCOMPARE(provider->value(QStringLiteral("identity")).toMap().value(QStringLiteral("region")),
             QStringLiteral("BigModel CN · Team"));
}

void ZaiUsageParserTest::mapsCreditQuotaRate()
{
    const QByteArray payload = R"({"code":200,"success":true,"data":{"limits":[
      {"type":"CREDIT_LIMIT","unit":3,"number":5,"percentage":10}
    ]}})";
    auto provider =
        ZaiUsageParser::parseQuota(payload, ZaiRegion::Global, ZaiUsageScope::Personal,
                                   QDateTime(QDate(2026, 8, 4), QTime(7, 0), QTimeZone::UTC));
    QVERIFY(provider.has_value());
    QVariantList rows = provider->value(QStringLiteral("details"))
                            .toList()
                            .first()
                            .toMap()
                            .value(QStringLiteral("rows"))
                            .toList();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("label")), QStringLiteral("Quota rate"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("value")), QStringLiteral("Peak"));

    provider =
        ZaiUsageParser::parseQuota(payload, ZaiRegion::Global, ZaiUsageScope::Personal,
                                   QDateTime(QDate(2026, 8, 8), QTime(7, 0), QTimeZone::UTC));
    QVERIFY(provider.has_value());
    rows = provider->value(QStringLiteral("details"))
               .toList()
               .first()
               .toMap()
               .value(QStringLiteral("rows"))
               .toList();
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("value")), QStringLiteral("Off-peak"));
}

void ZaiUsageParserTest::handlesTimeOnlyAndUnknownLimits()
{
    auto provider = ZaiUsageParser::parseQuota(
        R"({"code":200,"success":true,"data":{"limits":[
          {"type":"TIME_LIMIT","unit":3,"number":5,"percentage":22}
        ]}})",
        ZaiRegion::Global, ZaiUsageScope::Personal, QDateTime::currentDateTimeUtc());
    QVERIFY(provider.has_value());
    QVariantMap mcp = windowByKind(*provider, QStringLiteral("mcp"));
    QCOMPARE(mcp.value(QStringLiteral("windowSeconds")).toInt(), 18'000);
    QCOMPARE(provider->value(QStringLiteral("windows")).toList().size(), 1);

    provider = ZaiUsageParser::parseQuota(
        R"({"code":200,"success":true,"data":{"limits":[
          {"type":"IGNORED_LIMIT","unit":3,"number":5,"percentage":80},
          {"type":"TIME_LIMIT","unit":0,"number":1,"percentage":20}
        ]}})",
        ZaiRegion::Global, ZaiUsageScope::Personal, QDateTime::currentDateTimeUtc());
    QVERIFY(provider.has_value());
    mcp = windowByKind(*provider, QStringLiteral("mcp"));
    QVERIFY(!mcp.contains(QStringLiteral("windowSeconds")));
    QCOMPARE(mcp.value(QStringLiteral("usedPercent")).toDouble(), 20.0);

    provider = ZaiUsageParser::parseQuota(
        R"({"code":200,"success":true,"data":{"level":"Max","limits":[
          {"type":"TOKENS_LIMIT","unit":1,"number":30,"percentage":10},
          {"type":"TOKENS_LIMIT","unit":3,"number":0,"percentage":20},
          {"type":"TOKENS_LIMIT","unit":1,"number":2147483647,"percentage":30}
        ]}})",
        ZaiRegion::BigModelChina, ZaiUsageScope::Personal, QDateTime::currentDateTimeUtc());
    QVERIFY(provider.has_value());
    QCOMPARE(provider->value(QStringLiteral("identity")).toMap().value(QStringLiteral("plan")),
             QStringLiteral("Max"));
    QCOMPARE(provider->value(QStringLiteral("identity")).toMap().value(QStringLiteral("region")),
             QStringLiteral("BigModel CN"));
    const QVariantMap monthly = windowByKind(*provider, QStringLiteral("tertiary"));
    QCOMPARE(monthly.value(QStringLiteral("label")), QStringLiteral("30-day usage"));
    QCOMPARE(monthly.value(QStringLiteral("windowSeconds")).toInt(), 30 * 24 * 60 * 60);

    provider = ZaiUsageParser::parseQuota(
        R"({"code":200,"success":true,"data":{"limits":[
          {"type":"IGNORED_LIMIT","unit":3,"number":5,"percentage":80}
        ]}})",
        ZaiRegion::Global, ZaiUsageScope::Personal, QDateTime::currentDateTimeUtc());
    QVERIFY(provider.has_value());
    QCOMPARE(provider->value(QStringLiteral("status")).toMap().value(QStringLiteral("label")),
             QStringLiteral("No quota limits"));
}

void ZaiUsageParserTest::rejectsMalformedQuotaResponses()
{
    const QList<QPair<QByteArray, QString>> cases{
        {"{", QStringLiteral("z.ai quota API returned invalid JSON")},
        {"[]", QStringLiteral("z.ai quota API returned an invalid object")},
        {R"({"code":200,"success":false,"msg":"denied"})",
         QStringLiteral("z.ai quota API error: denied")},
        {R"({"code":500,"success":true})",
         QStringLiteral("z.ai quota API returned an unsuccessful status")},
        {R"({"code":200,"success":true,"data":{}})",
         QStringLiteral("z.ai quota API returned no limits")},
        {R"({"code":200,"success":true,"data":{"limits":[null]}})",
         QStringLiteral("z.ai quota API returned a malformed limit")},
        {R"({"code":200,"success":true,"data":{"limits":[{
          "type":"TOKENS_LIMIT","unit":3,"number":5,"percentage":1.5}]}})",
         QStringLiteral("z.ai quota API returned a malformed limit")},
        {R"({"code":200,"success":true,"data":{"limits":[{
          "type":"TOKENS_LIMIT","unit":3,"number":5,"percentage":5,"usage":"bad"}]}})",
         QStringLiteral("z.ai limit usage must be an integer")},
        {R"({"code":200,"success":true,"data":{"limits":[{
          "type":"TIME_LIMIT","unit":3,"number":5,"percentage":5,"usageDetails":{}}]}})",
         QStringLiteral("z.ai usageDetails must be an array")},
    };
    for (const auto &[payload, expected] : cases) {
        QString error;
        QVERIFY(!ZaiUsageParser::parseQuota(payload, ZaiRegion::Global, ZaiUsageScope::Personal,
                                            QDateTime::currentDateTimeUtc(), &error)
                     .has_value());
        QCOMPARE(error, expected);
    }

    const auto ignoredDetail = ZaiUsageParser::parseQuota(
        R"({"code":200,"success":true,"data":{"limits":[{
          "type":"TIME_LIMIT","unit":3,"number":5,"percentage":5,
          "usageDetails":[null,{"modelCode":"model","usage":1.5}]}]}})",
        ZaiRegion::Global, ZaiUsageScope::Personal, QDateTime::currentDateTimeUtc());
    QVERIFY(ignoredDetail.has_value());
    QCOMPARE(ignoredDetail->value(QStringLiteral("details"))
                 .toList()
                 .first()
                 .toMap()
                 .value(QStringLiteral("rows"))
                 .toList()
                 .size(),
             1);
}

void ZaiUsageParserTest::parsesModelUsage()
{
    const QByteArray payload = R"({"code":200,"success":true,"data":{
      "x_time":["08:00","09:00"],
      "modelDataList":[
        {"modelName":"glm-4.6","tokensUsage":[100,null]},
        {"modelName":"glm-4.5","tokensUsage":[50,25]},
        {"modelName":"unused","tokensUsage":[0,-2]}
      ]}})";
    QString error;
    const auto usage = ZaiUsageParser::parseModelUsage(payload, &error);
    QVERIFY(usage.has_value());
    QVERIFY(error.isEmpty());
    QCOMPARE(usage->points.size(), 2);
    QCOMPARE(usage->points.at(0).toMap().value(QStringLiteral("label")), QStringLiteral("08:00"));
    QCOMPARE(usage->points.at(0).toMap().value(QStringLiteral("value")).toLongLong(), 150);
    QCOMPARE(usage->rows.size(), 2);
    QCOMPARE(usage->rows.at(0).toMap().value(QStringLiteral("label")), QStringLiteral("glm-4.6"));
    QCOMPARE(usage->rows.at(0).toMap().value(QStringLiteral("value")), QStringLiteral("100"));
}

void ZaiUsageParserTest::rejectsMalformedModelUsage()
{
    const QList<QByteArray> cases{
        "{",
        "[]",
        R"({"code":500,"success":true})",
        R"({"code":200,"success":true,"data":{"x_time":{},"modelDataList":[]}})",
        R"({"code":200,"success":true,"data":{"x_time":[],"modelDataList":{}}})",
        R"({"code":200,"success":true,"data":{"x_time":[],"modelDataList":[null]}})",
        R"({"code":200,"success":true,"data":{"x_time":[],"modelDataList":[{
          "modelName":"glm","tokensUsage":{}}]}})",
    };
    for (const QByteArray &payload : cases) {
        QString error;
        QVERIFY(!ZaiUsageParser::parseModelUsage(payload, &error).has_value());
        QVERIFY(!error.isEmpty());
    }
}

void ZaiUsageParserTest::parsesChinaBalance()
{
    QString error;
    auto balance = ZaiUsageParser::parseBalance(
        R"({"success":true,"data":{"availableBalance":"42.5","balance":40,
          "rechargeAmount":30,"giveAmount":"12.5","totalSpendAmount":8}})",
        &error);
    QVERIFY(balance.has_value());
    QCOMPARE(balance->available, 42.5);
    QCOMPARE(balance->recharged, 30.0);
    QCOMPARE(balance->granted, 12.5);
    QCOMPARE(balance->spent, 8.0);

    balance = ZaiUsageParser::parseBalance(
        R"({"success":true,"data":{"availableBalance":null,"balance":"9.5"}})");
    QVERIFY(balance.has_value());
    QCOMPARE(balance->available, 9.5);

    const QList<QByteArray> invalid{
        "{",
        "[]",
        R"({"success":false,"data":{}})",
        R"({"success":true,"data":{"availableBalance":null,"balance":null}})",
        R"({"success":true,"data":{"availableBalance":"bad"}})",
    };
    for (const QByteArray &payload : invalid) {
        QVERIFY(!ZaiUsageParser::parseBalance(payload, &error).has_value());
        QVERIFY(!error.isEmpty());
    }
}

QTEST_GUILESS_MAIN(ZaiUsageParserTest)
#include "tst_zai_usage_parser.moc"
