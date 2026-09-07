#include <kodometer/cost_history_model.hpp>

#include <QDate>
#include <QtTest>
#include <limits>

using Kodometer::CostHistoryModel;

namespace {
QVariantMap point(const QString &date, const QVariant &amount)
{
    return {{QStringLiteral("label"), date}, {QStringLiteral("value"), amount}};
}

QVariantMap history(const QVariantList &daily = {})
{
    return {{QStringLiteral("currencyCode"), QStringLiteral("USD")},
            {QStringLiteral("historyEndDate"), QStringLiteral("2027-01-15")},
            {QStringLiteral("daily"), daily}};
}

QVariantList points(const CostHistoryModel &model)
{
    return model.view().value(QStringLiteral("points")).toList();
}
} // namespace

class CostHistoryModelTest final : public QObject
{
    Q_OBJECT
  private slots:
    void defaultsAndChangeSignals()
    {
        CostHistoryModel model;
        QSignalSpy costChanged(&model, &CostHistoryModel::costChanged);
        QSignalSpy daysChanged(&model, &CostHistoryModel::daysChanged);
        QSignalSpy viewChanged(&model, &CostHistoryModel::viewChanged);
        QVERIFY(model.cost().isEmpty());
        QVERIFY(model.view().isEmpty());
        QCOMPARE(model.days(), 30);
        model.setCost({});
        model.setDays(30);
        QCOMPARE(costChanged.count(), 0);
        QCOMPARE(daysChanged.count(), 0);
        QCOMPARE(viewChanged.count(), 0);
        model.setDays(7);
        QCOMPARE(model.days(), 7);
        QCOMPARE(daysChanged.count(), 1);
        model.setDays(1); // Only supported choices are 7 and 30; invalid choices use 30.
        QCOMPARE(model.days(), 30);
        model.setDays(90);
        QCOMPARE(daysChanged.count(), 2);
        const QVariantMap cost = history();
        model.setCost(cost);
        QCOMPARE(model.cost(), cost);
        QCOMPARE(costChanged.count(), 1);
        QCOMPARE(viewChanged.count(), 1);
        model.setCost(cost);
        QCOMPARE(costChanged.count(), 1);
        model.setDays(7);
        QCOMPARE(viewChanged.count(), 2);
        QCOMPARE(points(model).size(), 7);
        model.setCost({});
        QVERIFY(model.view().isEmpty());
        QCOMPARE(viewChanged.count(), 3);
    }

    void sortsAndBoundsCalendarDaysWithoutInventingZeros()
    {
        CostHistoryModel model;
        model.setDays(7);
        model.setCost(history(
            {point(QStringLiteral("2027-01-15"), 2.0), point(QStringLiteral("2027-01-09"), 1),
             point(QStringLiteral("2027-01-11"), 0), point(QStringLiteral("2027-01-08"), 500),
             point(QStringLiteral("2027-01-16"), 900)}));
        const QVariantMap view = model.view();
        const QVariantList rows = points(model);
        QCOMPARE(rows.size(), 7);
        QCOMPARE(rows.first().toMap().value(QStringLiteral("date")).toString(),
                 QStringLiteral("2027-01-09"));
        QCOMPARE(rows.last().toMap().value(QStringLiteral("date")).toString(),
                 QStringLiteral("2027-01-15"));
        QCOMPARE(rows.first().toMap().value(QStringLiteral("fraction")).toDouble(), 0.5);
        QCOMPARE(rows.last().toMap().value(QStringLiteral("fraction")).toDouble(), 1.0);
        QVERIFY(rows.at(1).toMap().value(QStringLiteral("amount")).isNull());
        QCOMPARE(rows.at(1).toMap().value(QStringLiteral("description")).toString(),
                 QStringLiteral("2027-01-10: Not reported"));
        QVERIFY(!rows.at(2).toMap().value(QStringLiteral("amount")).isNull());
        QCOMPARE(rows.at(2).toMap().value(QStringLiteral("description")).toString(),
                 QStringLiteral("2027-01-11: $0.00"));
        QCOMPARE(view.value(QStringLiteral("summary")).toString(),
                 QStringLiteral("Reported spend: $3.00 · 3/7 days reported"));
        QCOMPARE(view.value(QStringLiteral("rangeLabel")).toString(),
                 QStringLiteral("2027-01-09 – 2027-01-15 (UTC)"));
        QVERIFY(view.value(QStringLiteral("partial")).toBool());
        QVERIFY(view.value(QStringLiteral("available")).toBool());
        QVERIFY(!view.value(QStringLiteral("includesCurrentDay")).toBool());
        model.setDays(30);
        QCOMPARE(points(model).size(), 30);
        QCOMPARE(points(model).first().toMap().value(QStringLiteral("date")).toString(),
                 QStringLiteral("2026-12-17"));
        QVERIFY(model.view()
                    .value(QStringLiteral("summary"))
                    .toString()
                    .contains(QStringLiteral("$503.00")));
    }

    void emptyAndZeroAreDifferent()
    {
        CostHistoryModel model;
        model.setDays(7);
        model.setCost(history());
        QCOMPARE(model.view().value(QStringLiteral("summary")).toString(),
                 QStringLiteral("No daily spend reported"));
        QVERIFY(model.view().value(QStringLiteral("partial")).toBool());
        QVariantList zeros;
        for (int day = 9; day <= 15; ++day) {
            zeros.append(point(QDate(2027, 1, day).toString(Qt::ISODate), 0.0));
        }
        QVariantMap cost = history(zeros);
        model.setCost(cost);
        QVERIFY(!model.view().value(QStringLiteral("partial")).toBool());
        QCOMPARE(model.view().value(QStringLiteral("summary")).toString(),
                 QStringLiteral("Reported spend: $0.00 · 7/7 days reported"));
        QCOMPARE(points(model).first().toMap().value(QStringLiteral("fraction")).toDouble(), 0.0);
        cost.insert(QStringLiteral("historyPartial"), true);
        cost.insert(QStringLiteral("historyIncludesCurrentDay"), true);
        model.setCost(cost);
        QVERIFY(model.view().value(QStringLiteral("partial")).toBool());
        QVERIFY(model.view().value(QStringLiteral("includesCurrentDay")).toBool());
    }

    void tinyAmountsAndNumericTypes()
    {
        for (const QVariant &value : {QVariant(1), QVariant(1U), QVariant(1LL), QVariant(1ULL),
                                      QVariant(1.0F), QVariant(1.0)}) {
            CostHistoryModel model;
            model.setCost(history({point(QStringLiteral("2027-01-15"), value)}));
            QVERIFY(model.view().value(QStringLiteral("available")).toBool());
        }
        CostHistoryModel model;
        model.setCost(history({point(QStringLiteral("2027-01-15"), 0.000001)}));
        QCOMPARE(points(model).last().toMap().value(QStringLiteral("description")).toString(),
                 QStringLiteral("2027-01-15: <$0.01"));
        QVERIFY(model.view()
                    .value(QStringLiteral("summary"))
                    .toString()
                    .contains(QStringLiteral("<$0.01")));
    }

    void revalidatesEqualButDifferentlyTypedData()
    {
        CostHistoryModel model;
        model.setCost(history({point(QStringLiteral("2027-01-15"), 3)}));
        QVERIFY(!model.view().isEmpty());
        model.setCost(history({point(QStringLiteral("2027-01-15"), QStringLiteral("3"))}));
        QVERIFY(model.view().isEmpty());
    }

    void handlesLeapDaysAndUTCMetadata()
    {
        CostHistoryModel model;
        model.setDays(7);
        QVariantMap cost = history({point(QStringLiteral("2028-02-29"), 1)});
        cost.insert(QStringLiteral("historyEndDate"), QStringLiteral("2028-03-01"));
        model.setCost(cost);
        QCOMPARE(points(model).at(5).toMap().value(QStringLiteral("date")).toString(),
                 QStringLiteral("2028-02-29"));
        cost.insert(QStringLiteral("historyEstimated"), true);
        model.setCost(cost);
        QVERIFY(model.view().value(QStringLiteral("estimated")).toBool());
    }

    void rejectsMalformedHistory_data()
    {
        QTest::addColumn<QVariantMap>("cost");
        for (const QVariant &value : {QVariant(), QVariant(QMetaType::fromType<double>()),
                                      QVariant(false), QVariant(QStringLiteral("3")), QVariant(-1),
                                      QVariant(std::numeric_limits<double>::infinity()),
                                      QVariant(std::numeric_limits<double>::quiet_NaN())}) {
            QTest::newRow(qPrintable(
                QStringLiteral("amount-%1-%2")
                    .arg(value.typeName() ? value.typeName() : "null", value.toString())))
                << history({point(QStringLiteral("2027-01-15"), value)});
        }
        QTest::newRow("invalid-row") << history({QStringLiteral("bad")});
        QTest::newRow("invalid-date") << history({point(QStringLiteral("2027-02-30"), 1)});
        QTest::newRow("timestamp-not-date")
            << history({point(QStringLiteral("2027-01-15T00:00:00Z"), 1)});
        QTest::newRow("duplicate") << history(
            {point(QStringLiteral("2027-01-15"), 1), point(QStringLiteral("2027-01-15"), 1)});
        QTest::newRow("overflow") << history({point(QStringLiteral("2027-01-14"), 1e308),
                                              point(QStringLiteral("2027-01-15"), 1e308)});
        QTest::newRow("oversized")
            << history(QVariantList(367, point(QStringLiteral("2027-01-15"), 1)));
        for (const QString &key : {QStringLiteral("daily"), QStringLiteral("historyEndDate"),
                                   QStringLiteral("currencyCode")}) {
            QVariantMap cost = history();
            cost.remove(key);
            QTest::newRow(qPrintable(QStringLiteral("missing-") + key)) << cost;
            cost.insert(key, QStringLiteral("invalid"));
            QTest::newRow(qPrintable(QStringLiteral("invalid-") + key)) << cost;
        }
        QVariantMap currency = history();
        currency.insert(QStringLiteral("currencyCode"), QStringLiteral("CNY"));
        QTest::newRow("non-usd") << currency;
        QVariantMap nullList = history();
        nullList.insert(QStringLiteral("daily"), QVariant(QMetaType::fromType<QVariantList>()));
        QTest::newRow("null-list") << nullList;
        QVariantMap ancient = history();
        ancient.insert(QStringLiteral("historyEndDate"), QStringLiteral("0001-01-01"));
        QTest::newRow("unrepresentable-start-date") << ancient;
    }

    void rejectsMalformedHistory()
    {
        QFETCH(QVariantMap, cost);
        CostHistoryModel model;
        model.setCost(history());
        model.setCost(cost);
        QVERIFY(model.view().isEmpty());
    }
};

QTEST_GUILESS_MAIN(CostHistoryModelTest)
#include "tst_cost_history_model.moc"
