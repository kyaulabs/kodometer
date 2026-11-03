#include <kodometer/cost_history_model.hpp>
#include <kodometer/openai_costs.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using Kodometer::OpenAICosts;

namespace {
QJsonObject bucket(qint64 start, QJsonValue amount = 1.25)
{
    return {{"object", "bucket"},
            {"start_time", start},
            {"end_time", start + 86400},
            {"results", QJsonArray{QJsonObject{
                            {"object", "organization.costs.result"},
                            {"amount", QJsonObject{{"value", amount}, {"currency", "usd"}}}}}}};
}
QByteArray page(const QJsonArray &buckets)
{
    return QJsonDocument(QJsonObject{{"object", "page"},
                                     {"data", buckets},
                                     {"has_more", false},
                                     {"next_page", QJsonValue::Null}})
        .toJson(QJsonDocument::Compact);
}
} // namespace

class OpenAICostsTest : public QObject
{
    Q_OBJECT
  private slots:
    void preservesReportedDaysAndUnknownBalance()
    {
        const QDateTime now(QDate(2026, 6, 12), QTime(12, 0), QTimeZone::UTC);
        const qint64 start =
            QDateTime(QDate(2026, 6, 1), QTime(), QTimeZone::UTC).toSecsSinceEpoch();
        const auto result =
            OpenAICosts::parse(page({bucket(start), bucket(start + 2 * 86400, 2.5)}), start, now);
        QVERIFY(result);
        const auto cost = result->value("cost").toMap();
        QCOMPARE(cost.value("daily").toList().size(), 2);
        QCOMPARE(cost.value("monthToDateUSD").toDouble(), 3.75);
        Kodometer::CostHistoryModel chart;
        chart.setCost(cost);
        QVERIFY(!chart.view().isEmpty());
        const auto bars = chart.view().value("points").toList();
        QCOMPARE(bars.size(), 30);
        int known = 0;
        for (const auto &bar : bars)
            known += !bar.toMap().value("amount").isNull();
        QCOMPARE(known, 2);
        QVERIFY(!cost.contains("balance"));
        QVERIFY(!cost.contains("balanceUSD"));
        QVERIFY(!cost.contains("todayUSD"));
        const auto empty = OpenAICosts::parse(page({}), start, now);
        QVERIFY(empty);
        QVERIFY(!empty->value("cost").toMap().contains("monthToDateUSD"));
        const qint64 today = QDateTime(now.date(), QTime(), QTimeZone::UTC).toSecsSinceEpoch();
        const auto current = OpenAICosts::parse(page({bucket(today, 0)}), start, now);
        QVERIFY(current);
        QCOMPARE(current->value("cost").toMap().value("todayUSD").toDouble(), 0.0);
        auto clipped = bucket(today, 2.0);
        clipped.insert("end_time", now.toSecsSinceEpoch());
        QVERIFY(OpenAICosts::parse(page({clipped}), start, now));
        clipped = bucket(start, 2.0);
        clipped.insert("end_time", now.toSecsSinceEpoch());
        QVERIFY(!OpenAICosts::parse(page({clipped}), start, now));
        const qint64 priorMonth =
            QDateTime(QDate(2026, 5, 31), QTime(), QTimeZone::UTC).toSecsSinceEpoch();
        const auto previous = OpenAICosts::parse(page({bucket(priorMonth)}), priorMonth, now);
        QVERIFY(previous);
        QVERIFY(!previous->value("cost").toMap().contains("monthToDateUSD"));
        auto unreported = bucket(today);
        unreported.insert("results", QJsonArray{});
        const auto missing = OpenAICosts::parse(page({unreported}), start, now);
        QVERIFY(missing);
        QVERIFY(missing->value("cost").toMap().value("daily").toList().isEmpty());
        QVERIFY(!OpenAICosts::parse(page({}), start, QDateTime{}));
    }
    void rejectsMalformedOrPartialPages_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        const qint64 start = 1780272000;
        QTest::newRow("json") << QByteArray("{");
        QTest::newRow("array") << QByteArray("[]");
        QTest::newRow("missing-fields") << QByteArray("{}");
        QTest::newRow("negative") << page({bucket(start, -1)});
        QTest::newRow("string") << page({bucket(start, "1.25")});
        QTest::newRow("duplicate") << page({bucket(start), bucket(start)});
        QTest::newRow("before-range") << page({bucket(start - 86400)});
        QTest::newRow("after-range") << page({bucket(start + 20 * 86400)});
        auto wrong = bucket(start);
        wrong.insert("end_time", start + 42);
        QTest::newRow("bad-width") << page({wrong});
        wrong = bucket(start);
        wrong.insert("object", "wrong");
        QTest::newRow("bucket-type") << page({wrong});
        wrong = bucket(start);
        wrong.insert("start_time", "1780272000");
        QTest::newRow("timestamp-type") << page({wrong});
        wrong = bucket(start);
        wrong.insert("start_time", start + 1);
        QTest::newRow("unaligned") << page({wrong});
        wrong = bucket(start);
        wrong.insert("results", false);
        QTest::newRow("results-type") << page({wrong});
        wrong = bucket(start);
        const auto result = wrong.value("results").toArray().first();
        wrong.insert("results", QJsonArray{result, result});
        QTest::newRow("grouped") << page({wrong});
        QJsonArray many;
        for (int index = 0; index < 63; ++index)
            many.append(bucket(start));
        QTest::newRow("too-many") << page(many);
        QTest::newRow("overflow") << page({bucket(start, 1.7e308), bucket(start + 86400, 1.7e308)});
        wrong = bucket(start);
        wrong.insert("results", QJsonArray{QJsonObject{
                                    {"object", "organization.costs.result"},
                                    {"amount", QJsonObject{{"value", 1}, {"currency", "eur"}}}}});
        QTest::newRow("currency") << page({wrong});
        auto root = QJsonDocument::fromJson(page({bucket(start)})).object();
        root.insert("data", false);
        QTest::newRow("data-type") << QJsonDocument(root).toJson();
        root.insert("data", QJsonArray{bucket(start)});
        root.insert("next_page", "unexpected");
        QTest::newRow("unexpected-cursor") << QJsonDocument(root).toJson();
        root.insert("next_page", QJsonValue::Null);
        root.insert("has_more", true);
        QTest::newRow("partial") << QJsonDocument(root).toJson();
        root.insert("has_more", "false");
        QTest::newRow("has-more-type") << QJsonDocument(root).toJson();
    }
    void rejectsMalformedOrPartialPages()
    {
        QFETCH(QByteArray, bytes);
        QVERIFY(!OpenAICosts::parse(bytes, 1780272000,
                                    QDateTime(QDate(2026, 6, 12), QTime(12, 0), QTimeZone::UTC)));
    }
};
QTEST_GUILESS_MAIN(OpenAICostsTest)
#include "tst_openai_costs.moc"
