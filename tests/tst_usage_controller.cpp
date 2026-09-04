#include <kodometer/provider_adapter.hpp>
#include <kodometer/usage_controller.hpp>

#include <QtTest>

using Kodometer::ProviderAdapter;
using Kodometer::UsageController;

class FakeProviderAdapter final : public ProviderAdapter
{
  public:
    explicit FakeProviderAdapter(QString providerId) : m_providerId(std::move(providerId)) {}

    [[nodiscard]] QString providerId() const override
    {
        return m_providerId;
    }
    void refresh() override
    {
        ++refreshCount;
    }

    void succeed(const QVariantMap &provider)
    {
        emit refreshSucceeded(provider);
    }
    void fail(const QString &error)
    {
        emit refreshFailed(error);
    }

    int refreshCount = 0;

  private:
    QString m_providerId;
};

class UsageControllerTest final : public QObject
{
    Q_OBJECT

  private slots:
    void aggregatesProvidersInRegistrationOrder();
    void retainsLastGoodProviderOnFailure();
    void handlesNoProviders();
    void handlesRegistrationEdges();
};

void UsageControllerTest::aggregatesProvidersInRegistrationOrder()
{
    auto *codex = new FakeProviderAdapter(QStringLiteral("codex"));
    auto *claude = new FakeProviderAdapter(QStringLiteral("claude"));
    UsageController controller({codex, claude});
    QSignalSpy finished(&controller, &UsageController::refreshFinished);

    controller.refresh();
    controller.refresh();

    QVERIFY(controller.busy());
    QCOMPARE(codex->refreshCount, 1);
    QCOMPARE(claude->refreshCount, 1);
    claude->succeed({{QStringLiteral("id"), QStringLiteral("claude")}});
    QVERIFY(controller.busy());
    codex->succeed({{QStringLiteral("id"), QStringLiteral("codex")}});

    QVERIFY(!controller.busy());
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(controller.providers().size(), 2);
    QCOMPARE(controller.providers().at(0).toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("codex"));
    QCOMPARE(controller.providers().at(1).toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("claude"));
    QVERIFY(controller.error().isEmpty());
    QCOMPARE(controller.snapshot().value(QStringLiteral("providers")).toList(),
             controller.providers());
    QVERIFY(!controller.snapshot().value(QStringLiteral("generatedAt")).toString().isEmpty());
}

void UsageControllerTest::retainsLastGoodProviderOnFailure()
{
    auto *codex = new FakeProviderAdapter(QStringLiteral("codex"));
    auto *claude = new FakeProviderAdapter(QStringLiteral("claude"));
    UsageController controller({codex, claude});
    QSignalSpy finished(&controller, &UsageController::refreshFinished);

    controller.refresh();
    codex->succeed(
        {{QStringLiteral("id"), QStringLiteral("codex")}, {QStringLiteral("version"), 1}});
    claude->fail(QStringLiteral("Claude unavailable"));
    QCOMPARE(finished.takeFirst().first().toBool(), false);
    QCOMPARE(controller.providers().size(), 1);
    QCOMPARE(controller.error(), QStringLiteral("Claude: Claude unavailable"));

    controller.refresh();
    codex->fail(QStringLiteral("Codex unavailable"));
    claude->succeed({{QStringLiteral("id"), QStringLiteral("claude")}});

    QCOMPARE(finished.takeFirst().first().toBool(), false);
    QCOMPARE(controller.providers().size(), 2);
    QCOMPARE(controller.providers().at(0).toMap().value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(controller.error(), QStringLiteral("Codex: Codex unavailable"));
}

void UsageControllerTest::handlesRegistrationEdges()
{
    UsageController defaultController;
    QVERIFY(defaultController.providers().isEmpty());

    QObject owner;
    auto *emptyId = new FakeProviderAdapter({});
    emptyId->setParent(&owner);
    UsageController controller({nullptr, emptyId});
    QSignalSpy providersChanged(&controller, &UsageController::providersChanged);
    QSignalSpy finished(&controller, &UsageController::refreshFinished);

    controller.refresh();
    emptyId->fail(QStringLiteral("Unavailable"));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(controller.error(), QStringLiteral("Provider: Unavailable"));

    controller.refresh();
    emptyId->succeed({{QStringLiteral("id"), QStringLiteral("empty")}});
    QCOMPARE(providersChanged.count(), 1);
    controller.refresh();
    emptyId->succeed({{QStringLiteral("id"), QStringLiteral("empty")}});
    QCOMPARE(providersChanged.count(), 1);
}

void UsageControllerTest::handlesNoProviders()
{
    UsageController controller(QList<ProviderAdapter *>{});
    QSignalSpy finished(&controller, &UsageController::refreshFinished);

    controller.refresh();

    QVERIFY(!controller.busy());
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), true);
    QVERIFY(controller.providers().isEmpty());
}

QTEST_GUILESS_MAIN(UsageControllerTest)

#include "tst_usage_controller.moc"
