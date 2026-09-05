#include <kodometer/credential_store.hpp>
#include <kodometer/provider_adapter.hpp>

#include <QtTest>

using Kodometer::CredentialBackend;
using Kodometer::CredentialStore;
using Kodometer::ProviderAdapter;

class FakeCredentialBackend final : public CredentialBackend
{
  public:
    using CredentialBackend::CredentialBackend;

    void open() override
    {
        ++openCount;
    }

    std::optional<QMap<QString, QString>> readSecrets(const QStringList &keys,
                                                       QString *error) override
    {
        ++readCount;
        if (!readError.isEmpty()) {
            if (error != nullptr) {
                *error = readError;
            }
            return std::nullopt;
        }
        QMap<QString, QString> result;
        for (const QString &key : keys) {
            if (values.contains(key)) {
                result.insert(key, values.value(key));
            }
        }
        return result;
    }

    bool writeSecret(const QString &key, const QString &value, QString *error) override
    {
        if (!writeError.isEmpty()) {
            if (error != nullptr) {
                *error = writeError;
            }
            return false;
        }
        values.insert(key, value);
        emit changed();
        return true;
    }

    bool removeSecret(const QString &key, QString *error) override
    {
        if (!removeError.isEmpty()) {
            if (error != nullptr) {
                *error = removeError;
            }
            return false;
        }
        values.remove(key);
        emit changed();
        return true;
    }

    void finishOpen(bool success, const QString &error = {})
    {
        emit openFinished(success, error);
    }

    void close()
    {
        emit closed();
    }

    void notifyChanged()
    {
        emit changed();
    }

    QMap<QString, QString> values;
    QString readError;
    QString writeError;
    QString removeError;
    int openCount = 0;
    int readCount = 0;
};

class ExposedProviderAdapter final : public ProviderAdapter
{
  public:
    using ProviderAdapter::credentialEnvironment;

    QString providerId() const override
    {
        return QStringLiteral("fake");
    }

    void refresh() override {}
};

class CredentialStoreTest final : public QObject
{
    Q_OBJECT

  private slots:
    void opensLoadsAndTracksSecrets();
    void validatesWritesAndRemovals();
    void reportsBackendFailures();
    void clearsSecretsWhenWalletCloses();
    void appliesCredentialOverridesWithoutReplacingEnvironment();
};

void CredentialStoreTest::opensLoadsAndTracksSecrets()
{
    auto *backend = new FakeCredentialBackend;
    backend->values = {{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("deepseek")},
                       {QStringLiteral("not-allowed"), QStringLiteral("ignored")}};
    CredentialStore store(backend);
    QSignalSpy readyChanged(&store, &CredentialStore::readyChanged);
    QSignalSpy secretsChanged(&store, &CredentialStore::secretsChanged);

    store.open();
    store.open();
    QCOMPARE(backend->openCount, 1);
    QVERIFY(store.busy());
    backend->finishOpen(true);

    QVERIFY(store.ready());
    QVERIFY(!store.busy());
    QVERIFY(store.error().isEmpty());
    QCOMPARE(store.configuredKeys(), QStringList{QStringLiteral("DEEPSEEK_API_KEY")});
    QCOMPARE(store.secrets().value(QStringLiteral("DEEPSEEK_API_KEY")),
             QStringLiteral("deepseek"));
    QCOMPARE(readyChanged.count(), 1);
    QCOMPARE(secretsChanged.count(), 1);

    store.open();
    QCOMPARE(backend->openCount, 1);
    backend->values.insert(QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("router"));
    backend->notifyChanged();
    QCOMPARE(store.configuredKeys(),
             (QStringList{QStringLiteral("DEEPSEEK_API_KEY"),
                          QStringLiteral("OPENROUTER_API_KEY")}));
    QCOMPARE(secretsChanged.count(), 2);
}

void CredentialStoreTest::validatesWritesAndRemovals()
{
    auto *backend = new FakeCredentialBackend;
    CredentialStore store(backend);

    QVERIFY(!store.saveSecret(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("key")));
    QCOMPARE(store.error(), QStringLiteral("KWallet is not ready"));

    store.open();
    backend->finishOpen(true);
    QVERIFY(store.saveSecret(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("  secret  ")));
    QCOMPARE(backend->values.value(QStringLiteral("DEEPSEEK_API_KEY")),
             QStringLiteral("secret"));
    QVERIFY(store.hasSecret(QStringLiteral("DEEPSEEK_API_KEY")));

    QVERIFY(!store.saveSecret(QStringLiteral("UNKNOWN_KEY"), QStringLiteral("secret")));
    QCOMPARE(store.error(), QStringLiteral("Credential key is not supported"));
    QVERIFY(!store.saveSecret(QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral(" \t ")));
    QCOMPARE(store.error(), QStringLiteral("Credential value is empty"));
    QVERIFY(!store.saveSecret(QStringLiteral("KIMI_CODE_API_KEY"),
                              QStringLiteral("first\nsecond")));
    QCOMPARE(store.error(), QStringLiteral("Credential value contains invalid characters"));
    QVERIFY(!store.saveSecret(QStringLiteral("KIMI_CODE_API_KEY"),
                              QString(CredentialStore::MaximumSecretSize + 1, QLatin1Char('x'))));
    QCOMPARE(store.error(), QStringLiteral("Credential value exceeds the 64 KiB limit"));

    QVERIFY(store.removeSecret(QStringLiteral("DEEPSEEK_API_KEY")));
    QVERIFY(!store.hasSecret(QStringLiteral("DEEPSEEK_API_KEY")));
    QVERIFY(!store.removeSecret(QStringLiteral("UNKNOWN_KEY")));
    QCOMPARE(store.error(), QStringLiteral("Credential key is not supported"));
}

void CredentialStoreTest::reportsBackendFailures()
{
    auto *backend = new FakeCredentialBackend;
    CredentialStore store(backend);

    store.open();
    backend->finishOpen(false, QStringLiteral("Wallet access was denied"));
    QVERIFY(!store.ready());
    QVERIFY(!store.busy());
    QCOMPARE(store.error(), QStringLiteral("Wallet access was denied"));

    store.open();
    backend->readError = QStringLiteral("Could not read wallet entries");
    backend->finishOpen(true);
    QVERIFY(!store.ready());
    QCOMPARE(store.error(), QStringLiteral("Could not read wallet entries"));

    backend->readError.clear();
    store.open();
    backend->finishOpen(true);
    QVERIFY(store.ready());

    backend->writeError = QStringLiteral("Could not write wallet entry");
    QVERIFY(!store.saveSecret(QStringLiteral("Z_AI_API_KEY"), QStringLiteral("secret")));
    QCOMPARE(store.error(), QStringLiteral("Could not write wallet entry"));

    backend->removeError = QStringLiteral("Could not remove wallet entry");
    QVERIFY(!store.removeSecret(QStringLiteral("Z_AI_API_KEY")));
    QCOMPARE(store.error(), QStringLiteral("Could not remove wallet entry"));
}

void CredentialStoreTest::clearsSecretsWhenWalletCloses()
{
    auto *backend = new FakeCredentialBackend;
    backend->values.insert(QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("secret"));
    CredentialStore store(backend);
    QSignalSpy secretsChanged(&store, &CredentialStore::secretsChanged);

    store.open();
    backend->finishOpen(true);
    QVERIFY(store.ready());

    backend->close();

    QVERIFY(!store.ready());
    QVERIFY(store.secrets().isEmpty());
    QVERIFY(store.configuredKeys().isEmpty());
    QCOMPARE(store.error(), QStringLiteral("KWallet was closed"));
    QCOMPARE(secretsChanged.count(), 2);
}

void CredentialStoreTest::appliesCredentialOverridesWithoutReplacingEnvironment()
{
    ExposedProviderAdapter adapter;
    adapter.setCredentialOverrides(
        {{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("wallet-key")},
         {QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("wallet-router")}});

    const QMap<QString, QString> environment = adapter.credentialEnvironment(
        {{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("environment-key")},
         {QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("  ")},
         {QStringLiteral("UNRELATED"), QStringLiteral("value")}});

    QCOMPARE(environment.value(QStringLiteral("DEEPSEEK_API_KEY")),
             QStringLiteral("environment-key"));
    QCOMPARE(environment.value(QStringLiteral("OPENROUTER_API_KEY")),
             QStringLiteral("wallet-router"));
    QCOMPARE(environment.value(QStringLiteral("UNRELATED")), QStringLiteral("value"));

    adapter.setCredentialOverrides({});
    QCOMPARE(adapter.credentialEnvironment({}), QMap<QString, QString>{});
}

QTEST_GUILESS_MAIN(CredentialStoreTest)

#include "tst_credential_store.moc"
