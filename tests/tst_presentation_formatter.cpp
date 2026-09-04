#include <codexbar/presentation_formatter.hpp>

#include <QtTest>

using CodexBar::PresentationFormatter;

class PresentationFormatterTest final : public QObject
{
    Q_OBJECT

  private slots:
    void clampsPercent_data();
    void clampsPercent();
    void formatsRemaining_data();
    void formatsRemaining();
    void formatsResetTime_data();
    void formatsResetTime();
    void rejectsInvalidResetTime();
    void choosesWindowTitle_data();
    void choosesWindowTitle();
    void formatsAmounts();
};

void PresentationFormatterTest::clampsPercent_data()
{
    QTest::addColumn<double>("input");
    QTest::addColumn<double>("expected");

    QTest::newRow("below") << -2.0 << 0.0;
    QTest::newRow("inside") << 42.5 << 42.5;
    QTest::newRow("above") << 120.0 << 100.0;
}

void PresentationFormatterTest::clampsPercent()
{
    QFETCH(double, input);
    QFETCH(double, expected);
    QCOMPARE(PresentationFormatter::clampPercent(input), expected);
}

void PresentationFormatterTest::formatsRemaining_data()
{
    QTest::addColumn<QVariant>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("missing") << QVariant() << QStringLiteral("—");
    QTest::newRow("null") << QVariant::fromValue(nullptr) << QStringLiteral("—");
    QTest::newRow("whole") << QVariant(72) << QStringLiteral("72% left");
    QTest::newRow("fraction") << QVariant(72.4) << QStringLiteral("72.4% left");
    QTest::newRow("clamped") << QVariant(140) << QStringLiteral("100% left");
}

void PresentationFormatterTest::formatsRemaining()
{
    QFETCH(QVariant, input);
    QFETCH(QString, expected);
    QCOMPARE(PresentationFormatter::remainingLabel(input), expected);
}

void PresentationFormatterTest::formatsResetTime_data()
{
    QTest::addColumn<QString>("timestamp");
    QTest::addColumn<QString>("expected");

    QTest::newRow("past") << QStringLiteral("2026-01-01T11:59:00Z") << QStringLiteral("now");
    QTest::newRow("seconds") << QStringLiteral("2026-01-01T12:00:45Z") << QStringLiteral("in 45s");
    QTest::newRow("minutes") << QStringLiteral("2026-01-01T12:02:05Z") << QStringLiteral("in 2m");
    QTest::newRow("hours") << QStringLiteral("2026-01-01T14:05:00Z") << QStringLiteral("in 2h 5m");
    QTest::newRow("days") << QStringLiteral("2026-01-03T15:00:00Z") << QStringLiteral("in 2d 3h");
}

void PresentationFormatterTest::formatsResetTime()
{
    QFETCH(QString, timestamp);
    QFETCH(QString, expected);
    const QDateTime now =
        QDateTime::fromString(QStringLiteral("2026-01-01T12:00:00Z"), Qt::ISODate);

    QCOMPARE(PresentationFormatter::resetLabelAt(timestamp, now), expected);
}

void PresentationFormatterTest::rejectsInvalidResetTime()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();

    QVERIFY(PresentationFormatter::resetLabelAt({}, now).isEmpty());
    QVERIFY(PresentationFormatter::resetLabelAt(QStringLiteral("tomorrow"), now).isEmpty());
    QVERIFY(PresentationFormatter::resetLabelAt(QStringLiteral("2026-01-01T12:00:00Z"), QDateTime())
                .isEmpty());
}

void PresentationFormatterTest::choosesWindowTitle_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("label");
    QTest::addColumn<QString>("expected");

    QTest::newRow("explicit") << QStringLiteral("weekly") << QStringLiteral("Seven day")
                              << QStringLiteral("Seven day");
    QTest::newRow("session") << QStringLiteral("session") << QString() << QStringLiteral("Session");
    QTest::newRow("weekly") << QStringLiteral("weekly") << QString() << QStringLiteral("Weekly");
    QTest::newRow("tertiary") << QStringLiteral("tertiary") << QString()
                              << QStringLiteral("Monthly");
    QTest::newRow("custom") << QStringLiteral("model-cap") << QString()
                            << QStringLiteral("Model cap");
    QTest::newRow("missing") << QString() << QString() << QStringLiteral("Usage");
}

void PresentationFormatterTest::choosesWindowTitle()
{
    QFETCH(QString, kind);
    QFETCH(QString, label);
    QFETCH(QString, expected);

    QCOMPARE(PresentationFormatter::windowTitle(kind, label), expected);
}

void PresentationFormatterTest::formatsAmounts()
{
    QCOMPARE(PresentationFormatter::creditsLabel(112.0, QStringLiteral("credits")),
             QStringLiteral("112 credits"));
    QCOMPARE(PresentationFormatter::creditsLabel(12.45, QStringLiteral("USD")),
             QStringLiteral("$12.45"));
    QCOMPARE(PresentationFormatter::creditsLabel(1.5, QStringLiteral("requests")),
             QStringLiteral("1.5 requests"));
    QCOMPARE(PresentationFormatter::usdLabel(1.0), QStringLiteral("$1.00"));
    QCOMPARE(PresentationFormatter::usdLabel(18.225), QStringLiteral("$18.23"));
}

QTEST_GUILESS_MAIN(PresentationFormatterTest)

#include "tst_presentation_formatter.moc"
