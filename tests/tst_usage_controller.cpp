#include <kodometer/credential_store.hpp>
#include <kodometer/provider_adapter.hpp>
#include <kodometer/usage_controller.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
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

    void setProfileDirectory(const QString &directory) override
    {
        profileDirectory = directory;
        ++profileChanges;
    }

    int refreshCount = 0;
    int profileChanges = 0;
    QString profileDirectory;

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
        lastEnvironment = environmentWithCredentialOverrides(baseEnvironment);
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
    void handlesCredentialStoreLifecycle();
    void schedulesRefreshWithoutOverlap();
    void filtersDisabledProviders();
    void changesProvidersDuringRefresh();
    void handlesSynchronousCompletion();
    void publishesOnlyFreshEnabledResults();
    void isolatesSelectedProfilesAndInflightResults();
    void blocksMalformedProfilesWithoutBlockingOtherProviders();
    void appliesMultipleContextsAtomicallyAndRenamesWithoutFetching();
    void rejectsReentrantContextChanges_data();
    void rejectsReentrantContextChanges();
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
    UsageController controller(QList<ProviderAdapter *>{adapter});
    controller.setCredentialStore(&store);
    QCOMPARE(controller.credentialStore(), &store);
    controller.setCredentialStore(&store);

    adapter->baseEnvironment.insert(QStringLiteral("DEEPSEEK_API_KEY"),
                                    QStringLiteral("environment-key"));
    controller.refresh();
    QCOMPARE(adapter->refreshCount, 1);
    QCOMPARE(adapter->lastEnvironment.value(QStringLiteral("DEEPSEEK_API_KEY")),
             QStringLiteral("environment-key"));
    adapter->baseEnvironment.clear();
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
    QCOMPARE(controller.credentialStore(), nullptr);
    controller.refresh();
    QCOMPARE(adapter->refreshCount, 4);
    QVERIFY(adapter->lastEnvironment.value(QStringLiteral("DEEPSEEK_API_KEY")).isEmpty());
}

void UsageControllerTest::handlesCredentialStoreLifecycle()
{
    auto *adapter = new CredentialAwareAdapter;
    auto *firstBackend = new ControllerCredentialBackend;
    auto *firstStore = new CredentialStore(firstBackend);
    auto *secondBackend = new ControllerCredentialBackend;
    CredentialStore secondStore(secondBackend);
    UsageController controller(QList<ProviderAdapter *>{adapter});

    controller.setCredentialStore(firstStore);
    controller.setCredentialStore(&secondStore);
    firstBackend->update();
    QCOMPARE(adapter->refreshCount, 0);

    controller.setCredentialStore(firstStore);
    delete firstStore;
    QCOMPARE(controller.credentialStore(), nullptr);
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

void UsageControllerTest::schedulesRefreshWithoutOverlap()
{
    auto *adapter = new FakeProviderAdapter(QStringLiteral("codex"));
    UsageController controller(QList<ProviderAdapter *>{adapter});
    QSignalSpy settings(&controller, &UsageController::refreshSettingsChanged);
    auto *timer = controller.findChild<QTimer *>(QStringLiteral("refreshTimer"));
    QVERIFY(timer);
    QVERIFY(controller.autoRefresh());
    QCOMPARE(controller.refreshIntervalMinutes(), 5);
    QVERIFY(!timer->isActive());
    controller.setRefreshIntervalMinutes(0);
    QCOMPARE(controller.refreshIntervalMinutes(), 1);
    controller.setRefreshIntervalMinutes(-100);
    QCOMPARE(settings.count(), 1);
    controller.setRefreshIntervalMinutes(2000);
    QCOMPARE(controller.refreshIntervalMinutes(), 1440);
    controller.setAutoRefresh(true);
    controller.refresh();
    QVERIFY(!timer->isActive());
    adapter->succeed({{QStringLiteral("id"), QStringLiteral("codex")}});
    QVERIFY(timer->isActive());
    QVERIFY(timer->isSingleShot());
    QCOMPARE(timer->interval(), 86400000);
    controller.setRefreshIntervalMinutes(2);
    QCOMPARE(timer->interval(), 120000);
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout"));
    QCOMPARE(adapter->refreshCount, 2);
    QVERIFY(!timer->isActive());
    controller.refresh();
    QCOMPARE(adapter->refreshCount, 2);
    adapter->fail(QStringLiteral("Unavailable"));
    QVERIFY(timer->isActive());
    controller.setAutoRefresh(false);
    QVERIFY(!timer->isActive());
    controller.setAutoRefresh(false);
    controller.refresh();
    adapter->fail(QStringLiteral("Unavailable"));
    QVERIFY(!timer->isActive());
    controller.setAutoRefresh(true);
    QVERIFY(timer->isActive());
}

void UsageControllerTest::filtersDisabledProviders()
{
    auto *codex = new FakeProviderAdapter(QStringLiteral("codex"));
    auto *claude = new FakeProviderAdapter(QStringLiteral("claude"));
    UsageController controller({codex, claude});
    QSignalSpy changed(&controller, &UsageController::disabledProvidersChanged);
    QSignalSpy finished(&controller, &UsageController::refreshFinished);
    QVERIFY(controller.disabledProviders().isEmpty());
    controller.setDisabledProviders({QStringLiteral("claude"), QStringLiteral("claude")});
    QCOMPARE(controller.disabledProviders(), QStringList{QStringLiteral("claude")});
    controller.setDisabledProviders({QStringLiteral("claude")});
    QCOMPARE(changed.count(), 1);
    QCOMPARE(codex->refreshCount, 0); // Applying startup settings must not start networking.
    controller.refresh();
    QCOMPARE(codex->refreshCount, 1);
    QCOMPARE(claude->refreshCount, 0);
    codex->succeed({{QStringLiteral("id"), QStringLiteral("codex")}});
    QCOMPARE(controller.providers().size(), 1);
    controller.setDisabledProviders({QStringLiteral("codex"), QStringLiteral("claude")});
    QVERIFY(controller.providers().isEmpty());
    QVERIFY(controller.snapshot().value(QStringLiteral("providers")).toList().isEmpty());
    QVERIFY(!controller.busy());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(!controller.findChild<QTimer *>()->isActive());
    QCOMPARE(finished.count(), 2);
    controller.setDisabledProviders({QStringLiteral("unknown")});
    QCOMPARE(codex->refreshCount, 2);
    QCOMPARE(claude->refreshCount, 1);
    codex->fail(QStringLiteral("Unavailable"));
    claude->succeed({{QStringLiteral("id"), QStringLiteral("claude")}});
    QCOMPARE(controller.providers().size(), 2); // Last-good Codex retained.
}

void UsageControllerTest::changesProvidersDuringRefresh()
{
    auto *codex = new FakeProviderAdapter(QStringLiteral("codex"));
    auto *claude = new FakeProviderAdapter(QStringLiteral("claude"));
    UsageController controller({codex, claude});
    controller.refresh();
    codex->fail(QStringLiteral("Old error"));
    controller.setDisabledProviders({QStringLiteral("codex")});
    controller.setDisabledProviders({QStringLiteral("claude"), QStringLiteral("codex")});
    claude->succeed({{QStringLiteral("id"), QStringLiteral("claude")}});
    QCoreApplication::processEvents();
    QVERIFY(!controller.busy());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(controller.providers().isEmpty());
    QCOMPARE(codex->refreshCount, 1);
    QCOMPARE(claude->refreshCount, 1);
    // Unsolicited late completions must not republish disabled providers or corrupt counts.
    claude->fail(QStringLiteral("Late error"));
    codex->succeed({{QStringLiteral("id"), QStringLiteral("codex")}});
    QVERIFY(controller.error().isEmpty());
    QVERIFY(controller.providers().isEmpty());
    controller.setDisabledProviders({QStringLiteral("claude")});
    QCOMPARE(codex->refreshCount, 2);
    codex->succeed({{QStringLiteral("id"), QStringLiteral("codex")}});
    QCOMPARE(controller.providers().size(), 1);
}

void UsageControllerTest::handlesSynchronousCompletion()
{
    class ImmediateAdapter : public ProviderAdapter
    {
      public:
        QString providerId() const override
        {
            return QStringLiteral("immediate");
        }
        void refresh() override
        {
            emit refreshFailed(QStringLiteral("No credential"));
        }
    };
    auto *immediate = new ImmediateAdapter;
    auto *slow = new FakeProviderAdapter(QStringLiteral("slow"));
    UsageController controller({immediate, slow});
    controller.refresh();
    QVERIFY(controller.busy());
    slow->succeed({{QStringLiteral("id"), QStringLiteral("slow")}});
    QVERIFY(!controller.busy());
    QCOMPARE(controller.error(), QStringLiteral("Immediate: No credential"));
}

void UsageControllerTest::publishesOnlyFreshEnabledResults()
{
    auto *codex = new FakeProviderAdapter(QStringLiteral("codex"));
    auto *claude = new FakeProviderAdapter(QStringLiteral("claude"));
    UsageController controller({codex, claude});
    QSignalSpy fresh(&controller, &UsageController::providerRefreshed);
    const QVariantMap result{{QStringLiteral("id"), QStringLiteral("codex")}};
    controller.refresh();
    codex->succeed(result);
    claude->fail(QStringLiteral("Unavailable"));
    QCOMPARE(fresh.count(), 1);
    QCOMPARE(fresh.first().first().toMap(), result);
    controller.refresh();
    codex->fail(QStringLiteral("Unavailable"));
    controller.setDisabledProviders({QStringLiteral("claude")});
    claude->succeed({{QStringLiteral("id"), QStringLiteral("claude")}});
    QCOMPARE(fresh.count(), 1);
    codex->succeed(result); // Unsolicited result before the queued refresh is ignored.
    QCOMPARE(fresh.count(), 1);
    QTRY_COMPARE(codex->refreshCount, 3);
    codex->succeed(result); // Identical, but newly fetched data is still a fresh result.
    QCOMPARE(fresh.count(), 2);
}

void UsageControllerTest::isolatesSelectedProfilesAndInflightResults()
{
    auto *codex = new FakeProviderAdapter(QStringLiteral("codex"));
    UsageController controller(QList<ProviderAdapter *>{codex});
    auto *profiles = controller.profiles();
    QSignalSpy contexts(&controller, &UsageController::providerContextChanged);
    QSignalSpy fresh(&controller, &UsageController::providerRefreshed);
    QVERIFY(profiles->addProfile("codex", "Work", "/work"));
    const QString work = profiles->selectedId("codex");
    QCOMPARE(codex->refreshCount, 0); // Loading settings never starts networking.
    controller.refresh();
    QCOMPARE(codex->profileDirectory, QStringLiteral("/work"));
    codex->succeed({{"id", "codex"}, {"name", "Codex"}});
    QCOMPARE(controller.providers().first().toMap().value("profileName").toString(),
             QStringLiteral("Work"));
    controller.refresh();
    const int pathChanges = codex->profileChanges;
    QVERIFY(profiles->addProfile("codex", "Personal", "/personal"));
    QVERIFY(controller.providers().isEmpty());
    QVERIFY(controller.snapshot().value("providers").toList().isEmpty());
    QCOMPARE(codex->profileChanges, pathChanges); // Never redirect an active token rotation.
    QVERIFY(
        profiles->selectProfile("codex", work)); // Even A -> B -> A invalidates the old request.
    codex->succeed({{"id", "codex"}, {"old", true}});
    QCOMPARE(fresh.count(), 1);
    QVERIFY(controller.providers().isEmpty());
    QTRY_COMPARE(codex->refreshCount, 3);
    codex->fail(QStringLiteral("Work failure"));
    QVERIFY(controller.providers().isEmpty());
    QVERIFY(!controller.error().isEmpty());
    QVERIFY(profiles->selectProfile("codex", "default"));
    QVERIFY(controller.error().isEmpty());
    QTRY_COMPARE(codex->refreshCount, 4);
    QVERIFY(codex->profileDirectory.isEmpty());
    QVERIFY(profiles->selectProfile("codex", work));
    codex->fail(QStringLiteral("Old account error"));
    QVERIFY(controller.error().isEmpty());
    QTRY_COMPARE(codex->refreshCount, 5);
    codex->succeed({{"id", "codex"}});
    QCOMPARE(fresh.count(), 2);
    QVERIFY(contexts.count() >= 4);
    // Changes before the queued refresh runs coalesce into one request.
    QVERIFY(profiles->selectProfile("codex", "default"));
    QVERIFY(profiles->selectProfile("codex", work));
    QTRY_COMPARE(codex->refreshCount, 6);
    codex->succeed({{"id", "codex"}});
    QCoreApplication::processEvents();
    QCOMPARE(codex->refreshCount, 6);
}

void UsageControllerTest::blocksMalformedProfilesWithoutBlockingOtherProviders()
{
    auto *codex = new FakeProviderAdapter(QStringLiteral("codex"));
    auto *other = new FakeProviderAdapter(QStringLiteral("other"));
    UsageController controller({codex, other});
    controller.profiles()->setConfiguration(QStringLiteral("invalid"));
    controller.refresh();
    QCOMPARE(codex->refreshCount, 0);
    QCOMPARE(other->refreshCount, 1);
    other->succeed({{"id", "other"}});
    QCOMPARE(other->profileChanges, 0);
    controller.profiles()->setConfiguration(QStringLiteral("{}"));
    // A manual cycle can start before the profile refresh callback; still queue a follow-up.
    controller.refresh();
    QCoreApplication::processEvents();
    codex->succeed({{"id", "codex"}});
    other->succeed({{"id", "other"}});
    QTRY_COMPARE(codex->refreshCount, 2);
    codex->succeed({{"id", "codex"}});
    other->succeed({{"id", "other"}});
}

void UsageControllerTest::appliesMultipleContextsAtomicallyAndRenamesWithoutFetching()
{
    auto *codex = new FakeProviderAdapter(QStringLiteral("codex"));
    auto *claude = new FakeProviderAdapter(QStringLiteral("claude"));
    UsageController controller({codex, claude});
    QVERIFY(controller.profiles()->addProfile("codex", "Work", "/codex"));
    QVERIFY(controller.profiles()->addProfile("claude", "Work", "/claude"));
    controller.refresh();
    codex->succeed({{"id", "codex"}});
    claude->succeed({{"id", "claude"}});
    QSignalSpy contexts(&controller, &UsageController::providerContextChanged);
    QJsonObject data =
        QJsonDocument::fromJson(controller.profiles()->configuration().toUtf8()).object();
    QJsonObject row = data.value("codex").toArray().first().toObject();
    row.insert("name", "Renamed");
    data.insert("codex", QJsonArray{row});
    controller.profiles()->setConfiguration(QString::fromUtf8(QJsonDocument(data).toJson()));
    QCOMPARE(controller.providers().first().toMap().value("profileName").toString(),
             QStringLiteral("Renamed"));
    QCOMPARE(contexts.count(), 0);
    QCoreApplication::processEvents();
    QCOMPARE(codex->refreshCount, 1);
    bool atomic = true;
    connect(&controller, &UsageController::providersChanged, &controller,
            [&] { atomic = atomic && controller.providers().isEmpty(); });
    data.insert("selectedCodex", "default");
    data.insert("selectedClaude", "default");
    controller.profiles()->setConfiguration(QString::fromUtf8(QJsonDocument(data).toJson()));
    QVERIFY(atomic);
    QCOMPARE(contexts.count(), 2);
    QTRY_COMPARE(codex->refreshCount, 2);
    QCOMPARE(claude->refreshCount, 2);
}

void UsageControllerTest::rejectsReentrantContextChanges_data()
{
    QTest::addColumn<bool>("disable");
    QTest::newRow("profile-switch") << false;
    QTest::newRow("provider-disable") << true;
}

void UsageControllerTest::rejectsReentrantContextChanges()
{
    QFETCH(bool, disable);
    auto *adapter = new FakeProviderAdapter(QStringLiteral("codex"));
    UsageController controller(QList<ProviderAdapter *>{adapter});
    QSignalSpy fresh(&controller, &UsageController::providerRefreshed);
    bool changed = false;
    connect(&controller, &UsageController::providersChanged, &controller, [&] {
        if (!changed && !controller.providers().isEmpty()) {
            changed = true;
            if (disable) {
                controller.setDisabledProviders({QStringLiteral("codex")});
            }
            else {
                QVERIFY(controller.profiles()->addProfile("codex", "Work", "/work"));
            }
        }
    });
    controller.refresh();
    adapter->succeed({{"id", "codex"}});
    QVERIFY(changed);
    QVERIFY(controller.providers().isEmpty());
    QCOMPARE(fresh.count(), 0);
}

QTEST_GUILESS_MAIN(UsageControllerTest)

#include "tst_usage_controller.moc"
