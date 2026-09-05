#include <kodometer/credential_store.hpp>
#include <kodometer/provider_adapter.hpp>
#include <kodometer/usage_controller.hpp>

#include <QtTest>

using Kodometer::CredentialBackend;
using Kodometer::CredentialStore;
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

class ControllerCredentialBackend final : public CredentialBackend
{
  public:
    using CredentialBackend::CredentialBackend;

    void open() override {}

    std::optional<QMap<QString, QString>> readSecrets(const QStringList &, QString *) override
    {
        return values;
    }

    bool writeSecret(const QString &, const QString &, QString *) override
    {
        return false;
    }

    bool removeSecret(const QString &, QString *) override
    {
        return false;
    }

    void load()
    {
        emit openFinished(true, {});
    }

    void update()
    {
        emit changed();
    }

    QMap<QString, QString> values;
};

class CredentialAwareAdapter final : public ProviderAdapter
{
  public:
    QString providerId() const override
    {
        return QStringLiteral("deepseek");
    }

    void refresh() override
    {
        ++refreshCount;
        lastEnvironment = credentialEnvironment(baseEnvironment);
    }

    void succeed()
    {
        emit refreshSucceeded({{QStringLiteral("id"), QStringLiteral("deepseek")}});
    }

    QMap<QString, QString> baseEnvironment;
    QMap<QString, QString> lastEnvironment;
    int refreshCount = 0;
};

class UsageControllerTest final : public QObject
{
    Q_OBJECT

  private slots:
    void aggregatesProvidersInRegistrationOrder();
    void retainsLastGoodProviderOnFailure();
    void handlesNoProviders();
    void handlesRegistrationEdges();
    void refreshesAfterCredentialChanges();
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

void UsageControllerTest::refreshesAfterCredentialChanges()
{
    auto *adapter = new CredentialAwareAdapter;
    auto *backend = new ControllerCredentialBackend;
    CredentialStore store(backend);
    UsageController controller({adapter});
    controller.setCredentialStore(&store);

    controller.refresh();
    QCOMPARE(adapter->refreshCount, 1);
    backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("wallet-key"));
    store.open();
    backend->load();
    QCOMPARE(adapter->refreshCount, 1);

    adapter->succeed();
    QTRY_COMPARE(adapter->refreshCount, 2);
    QCOMPARE(adapter->lastEnvironment.value(QStringLiteral("DEEPSEEK_API_KEY")),
             QStringLiteral("wallet-key"));
    adapter->succeed();

    backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("rotated-key"));
    backend->update();
    QTRY_COMPARE(adapter->refreshCount, 3);
    QCOMPARE(adapter->lastEnvironment.value(QStringLiteral("DEEPSEEK_API_KEY")),
             QStringLiteral("rotated-key"));
    adapter->succeed();

    controller.setCredentialStore(nullptr);
    QCOMPARE(adapter->lastEnvironment.value(QStringLiteral("DEEPSEEK_API_KEY")),
             QStringLiteral("rotated-key"));
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
