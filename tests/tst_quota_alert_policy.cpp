#include <kodometer/quota_alert_policy.hpp>

#include <QtTest>
#include <limits>

using Kodometer::QuotaAlertPolicy;

namespace {
QVariantMap snapshot(const QString &id, const QVariant &remaining, bool idle = false)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("windows"),
             QVariantList{QVariantMap{{QStringLiteral("remainingPercent"), remaining},
                                      {QStringLiteral("idle"), idle}}}}};
}
} // namespace

class QuotaAlertPolicyTest final : public QObject
{
    Q_OBJECT
  private slots:
    void optInAndThresholdBounds()
    {
        QuotaAlertPolicy policy;
        QSignalSpy alerts(&policy, &QuotaAlertPolicy::alertReady);
        QSignalSpy settings(&policy, &QuotaAlertPolicy::settingsChanged);
        QVERIFY(!policy.enabled());
        QCOMPARE(policy.thresholdPercent(), 10);
        policy.observe(snapshot(QStringLiteral("codex"), 1));
        QCOMPARE(alerts.count(), 0);
        policy.setEnabled(false);
        QCOMPARE(settings.count(), 0);
        policy.setThresholdPercent(0);
        QCOMPARE(policy.thresholdPercent(), 1);
        policy.setThresholdPercent(-1);
        QCOMPARE(settings.count(), 1);
        policy.setThresholdPercent(100);
        QCOMPARE(policy.thresholdPercent(), 50);
        policy.setThresholdPercent(10);
        policy.setEnabled(true);
        policy.setEnabled(true);
        QCOMPARE(settings.count(), 4);
        QCOMPARE(alerts.count(), 0); // Settings never replay cached results.
        policy.observe(snapshot(QStringLiteral("codex"), 10.0));
        QCOMPARE(alerts.count(), 1);
        policy.setEnabled(false);
        policy.observe(snapshot(QStringLiteral("codex"), 0));
        policy.setEnabled(true);
        policy.observe(snapshot(QStringLiteral("codex"), 0));
        QCOMPARE(alerts.count(), 2);
        policy.setThresholdPercent(20);
        policy.observe(snapshot(QStringLiteral("codex"), 15));
        QCOMPARE(alerts.count(), 3);
    }

    void suppressesDuplicatesUntilRecovery()
    {
        QuotaAlertPolicy policy;
        policy.setEnabled(true);
        QSignalSpy alerts(&policy, &QuotaAlertPolicy::alertReady);
        policy.observe(snapshot(QStringLiteral("codex"), 50));
        policy.observe(snapshot(QStringLiteral("codex"), 10));
        policy.observe(snapshot(QStringLiteral("codex"), 5));
        policy.observe(snapshot(QStringLiteral("codex"), 0));
        policy.observe(snapshot(QStringLiteral("codex"), 11));
        policy.observe(snapshot(QStringLiteral("codex"), 10));
        QCOMPARE(alerts.count(), 1);
        policy.observe(snapshot(QStringLiteral("codex"), 15));
        policy.observe(snapshot(QStringLiteral("codex"), 9));
        QCOMPARE(alerts.count(), 2);
        policy.observe(snapshot(QStringLiteral("claude"), 8));
        QCOMPARE(alerts.count(), 3);
    }

    void usesMostConstrainedWindowWithoutPrivateText()
    {
        QuotaAlertPolicy policy;
        policy.setEnabled(true);
        QSignalSpy alerts(&policy, &QuotaAlertPolicy::alertReady);
        QVariantMap provider = snapshot(QStringLiteral("codex"), 80);
        provider.insert(QStringLiteral("name"), QStringLiteral("secret@example.test"));
        QVariantList windows = provider.value(QStringLiteral("windows")).toList();
        windows.append(
            QVariantMap{{QStringLiteral("remainingPercent"), 8.25},
                        {QStringLiteral("label"), QStringLiteral("<b>private account</b>")}});
        windows.append(
            QVariantMap{{QStringLiteral("remainingPercent"), 0}, {QStringLiteral("idle"), true}});
        provider.insert(QStringLiteral("windows"), windows);
        policy.observe(provider);
        QCOMPARE(alerts.count(), 1);
        QCOMPARE(alerts.first().at(0).toString(), QStringLiteral("Codex quota is low"));
        QCOMPARE(alerts.first().at(1).toString(),
                 QStringLiteral("Most constrained quota: 8.3% remaining."));
    }

    void ignoresMissingOrInvalidQuota_data()
    {
        QTest::addColumn<QVariant>("remaining");
        QTest::newRow("absent") << QVariant();
        QTest::newRow("typed-null") << QVariant(QMetaType::fromType<double>());
        QTest::newRow("string") << QVariant(QStringLiteral("5"));
        QTest::newRow("bool") << QVariant(false);
        QTest::newRow("negative") << QVariant(-1.0);
        QTest::newRow("over-100") << QVariant(101.0);
        QTest::newRow("nan") << QVariant(std::numeric_limits<double>::quiet_NaN());
        QTest::newRow("infinite") << QVariant(std::numeric_limits<double>::infinity());
        QTest::newRow("map") << QVariant(QVariantMap{});
    }

    void ignoresMissingOrInvalidQuota()
    {
        QFETCH(QVariant, remaining);
        QuotaAlertPolicy policy;
        policy.setEnabled(true);
        QSignalSpy alerts(&policy, &QuotaAlertPolicy::alertReady);
        policy.observe(snapshot(QStringLiteral("codex"), remaining));
        QCOMPARE(alerts.count(), 0);
        policy.observe(snapshot(QStringLiteral("codex"), 5));
        policy.observe(snapshot(QStringLiteral("codex"), remaining));
        policy.observe(snapshot(QStringLiteral("codex"), 5));
        QCOMPARE(alerts.count(), 1); // Invalid data must not rearm an existing low episode.
    }

    void ignoresIdleBalanceAndUnknownProviders()
    {
        QuotaAlertPolicy policy;
        policy.setEnabled(true);
        QSignalSpy alerts(&policy, &QuotaAlertPolicy::alertReady);
        policy.observe(snapshot(QStringLiteral("codex"), 0, true));
        policy.observe({{QStringLiteral("id"), QStringLiteral("deepseek")},
                        {QStringLiteral("cost"), QVariantMap{{QStringLiteral("balance"), 0}}}});
        policy.observe(snapshot(QStringLiteral("unknown"), 0));
        policy.observe({});
        QCOMPARE(alerts.count(), 0);
    }

    void acceptsNumericRepresentations()
    {
        for (const QVariant &number : {QVariant(5), QVariant(5U), QVariant(5LL), QVariant(5ULL),
                                       QVariant(5.0F), QVariant(5.0)}) {
            QuotaAlertPolicy policy;
            policy.setEnabled(true);
            QSignalSpy alerts(&policy, &QuotaAlertPolicy::alertReady);
            policy.observe(snapshot(QStringLiteral("codex"), number));
            QCOMPARE(alerts.count(), 1);
        }
    }

    void incompleteWindowsDoNotProveRecovery()
    {
        QuotaAlertPolicy policy;
        policy.setEnabled(true);
        QSignalSpy alerts(&policy, &QuotaAlertPolicy::alertReady);
        policy.observe(snapshot(QStringLiteral("codex"), 5));
        QVariantMap provider = snapshot(QStringLiteral("codex"), 80);
        QVariantList windows = provider.value(QStringLiteral("windows")).toList();
        windows.append(
            QVariantMap{{QStringLiteral("remainingPercent"), QStringLiteral("invalid")}});
        windows.append(QVariantMap{{QStringLiteral("remainingPercent"), 90}});
        provider.insert(QStringLiteral("windows"), windows);
        policy.observe(provider);
        policy.observe(snapshot(QStringLiteral("codex"), 5));
        QCOMPARE(alerts.count(), 1);
    }

    void forgetsOnlyTheChangedProvider()
    {
        QuotaAlertPolicy policy;
        policy.setEnabled(true);
        QSignalSpy alerts(&policy, &QuotaAlertPolicy::alertReady);
        policy.observe(snapshot(QStringLiteral("codex"), 1));
        policy.observe(snapshot(QStringLiteral("claude"), 1));
        policy.forgetProvider(QStringLiteral("unknown"));
        policy.forgetProvider(QStringLiteral("codex"));
        QCOMPARE(alerts.count(), 2);
        policy.observe(snapshot(QStringLiteral("claude"), 1));
        policy.observe(snapshot(QStringLiteral("codex"), 1));
        QCOMPARE(alerts.count(), 3);
    }

    void recognizesNativeProviders()
    {
        QuotaAlertPolicy policy;
        policy.setEnabled(true);
        QSignalSpy alerts(&policy, &QuotaAlertPolicy::alertReady);
        for (const QString &id :
             {QStringLiteral("codex"), QStringLiteral("claude"), QStringLiteral("gemini"),
              QStringLiteral("xai"), QStringLiteral("kimi"), QStringLiteral("deepseek"),
              QStringLiteral("zai"), QStringLiteral("openrouter")}) {
            policy.observe(snapshot(id, 0));
        }
        QCOMPARE(alerts.count(), 8);
    }
};

QTEST_GUILESS_MAIN(QuotaAlertPolicyTest)
#include "tst_quota_alert_policy.moc"
