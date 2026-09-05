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
        if (readFails || !readError.isEmpty()) {
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
        if (writeFails || !writeError.isEmpty()) {
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
        if (removeFails || !removeError.isEmpty()) {
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
    bool readFails = false;
    bool writeFails = false;
    bool removeFails = false;
    int openCount = 0;
    int readCount = 0;
};

class ExposedProviderAdapter final : public ProviderAdapter
{
  public:
    using ProviderAdapter::environmentWithCredentialOverrides;

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
    void rejectsInvalidStoredSecrets_data();
    void rejectsInvalidStoredSecrets();
    void clearsSecretsWhenWalletCloses();
    void appliesCredentialOverridesWithoutReplacingEnvironment();
    void preservesBackendOwnership();
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
    QCOMPARE(store.secrets().value(QStringLiteral("DEEPSEEK_API_KEY")), QStringLiteral("deepseek"));
    QCOMPARE(readyChanged.count(), 1);
    QCOMPARE(secretsChanged.count(), 1);

    store.open();
    QCOMPARE(backend->openCount, 1);
    backend->values.insert(QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("router"));
    backend->notifyChanged();
    QCOMPARE(store.configuredKeys(), (QStringList{QStringLiteral("DEEPSEEK_API_KEY"),
                                                  QStringLiteral("OPENROUTER_API_KEY")}));
    QCOMPARE(secretsChanged.count(), 2);
}

void CredentialStoreTest::validatesWritesAndRemovals()
{
    auto *backend = new FakeCredentialBackend;
    CredentialStore store(backend);

    QVERIFY(!store.saveSecret(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("key")));
    QCOMPARE(store.error(), QStringLiteral("KWallet is not ready"));
    QVERIFY(!store.removeSecret(QStringLiteral("DEEPSEEK_API_KEY")));
    QCOMPARE(store.error(), QStringLiteral("KWallet is not ready"));
    QVERIFY(!store.hasSecret(QStringLiteral("UNKNOWN_KEY")));

    store.open();
    backend->finishOpen(true);
    QVERIFY(store.saveSecret(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("  secret  ")));
    QCOMPARE(backend->values.value(QStringLiteral("DEEPSEEK_API_KEY")), QStringLiteral("secret"));
    QVERIFY(store.hasSecret(QStringLiteral("DEEPSEEK_API_KEY")));

    QVERIFY(!store.saveSecret(QStringLiteral("UNKNOWN_KEY"), QStringLiteral("secret")));
    QCOMPARE(store.error(), QStringLiteral("Credential key is not supported"));
    QVERIFY(!store.saveSecret(QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral(" \t ")));
    QCOMPARE(store.error(), QStringLiteral("Credential value is empty"));
    QVERIFY(
        !store.saveSecret(QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("first\nsecond")));
    QCOMPARE(store.error(), QStringLiteral("Credential value contains invalid characters"));
    QVERIFY(
        !store.saveSecret(QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("first\rsecond")));
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
    backend->finishOpen(false);
    QVERIFY(!store.ready());
    QVERIFY(!store.busy());
    QCOMPARE(store.error(), QStringLiteral("KWallet could not be opened"));

    store.open();
    backend->finishOpen(false, QStringLiteral("Wallet access was denied"));
    QCOMPARE(store.error(), QStringLiteral("Wallet access was denied"));

    store.open();
    backend->readFails = true;
    backend->finishOpen(true);
    QCOMPARE(store.error(), QStringLiteral("KWallet credentials could not be read"));

    store.open();
    backend->readFails = false;
    backend->readError = QStringLiteral("Could not read wallet entries");
    backend->finishOpen(true);
    QVERIFY(!store.ready());
    QCOMPARE(store.error(), QStringLiteral("Could not read wallet entries"));

    backend->readError.clear();
    store.open();
    backend->finishOpen(true);
    QVERIFY(store.ready());

    backend->writeFails = true;
    QVERIFY(!store.saveSecret(QStringLiteral("Z_AI_API_KEY"), QStringLiteral("secret")));
    QCOMPARE(store.error(), QStringLiteral("KWallet could not store the credential"));
    backend->writeFails = false;
    backend->writeError = QStringLiteral("Could not write wallet entry");
    QVERIFY(!store.saveSecret(QStringLiteral("Z_AI_API_KEY"), QStringLiteral("secret")));
    QCOMPARE(store.error(), QStringLiteral("Could not write wallet entry"));

    backend->writeError.clear();
    QVERIFY(store.saveSecret(QStringLiteral("Z_AI_API_KEY"), QStringLiteral("secret")));
    backend->removeFails = true;
    QVERIFY(!store.removeSecret(QStringLiteral("Z_AI_API_KEY")));
    QCOMPARE(store.error(), QStringLiteral("KWallet could not remove the credential"));
    backend->removeFails = false;
    backend->removeError = QStringLiteral("Could not remove wallet entry");
    QVERIFY(!store.removeSecret(QStringLiteral("Z_AI_API_KEY")));
    QCOMPARE(store.error(), QStringLiteral("Could not remove wallet entry"));

    backend->removeError.clear();
    backend->readFails = true;
    QVERIFY(!store.saveSecret(QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("secret")));
    QCOMPARE(store.error(), QStringLiteral("KWallet credentials could not be read"));
    QVERIFY(!store.removeSecret(QStringLiteral("Z_AI_API_KEY")));
}

void CredentialStoreTest::rejectsInvalidStoredSecrets_data()
{
    QTest::addColumn<QString>("secret");
    QTest::newRow("empty") << QStringLiteral("  ");
    QTest::newRow("line-feed") << QStringLiteral("first\nsecond");
    QTest::newRow("carriage-return") << QStringLiteral("first\rsecond");
    QTest::newRow("oversized") << QString(CredentialStore::MaximumSecretSize + 1, QLatin1Char('x'));
}

void CredentialStoreTest::rejectsInvalidStoredSecrets()
{
    QFETCH(QString, secret);
    auto *backend = new FakeCredentialBackend;
    backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), secret);
    CredentialStore store(backend);

    store.open();
    backend->finishOpen(true);

    QVERIFY(!store.ready());
    QVERIFY(store.secrets().isEmpty());
    QCOMPARE(store.error(),
             QStringLiteral("KWallet contains an invalid credential for DEEPSEEK_API_KEY"));
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

void CredentialStoreTest::preservesBackendOwnership()
{
    QObject owner;
    FakeCredentialBackend backend(&owner);
    CredentialStore store(&backend);

    QCOMPARE(backend.parent(), &owner);
}

void CredentialStoreTest::appliesCredentialOverridesWithoutReplacingEnvironment()
{
    ExposedProviderAdapter adapter;
    adapter.setCredentialOverrides(
        {{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("wallet-key")},
         {QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("wallet-router")}});

    const QMap<QString, QString> environment = adapter.environmentWithCredentialOverrides(
        {{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("environment-key")},
         {QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("  ")},
         {QStringLiteral("UNRELATED"), QStringLiteral("value")}});

    QCOMPARE(environment.value(QStringLiteral("DEEPSEEK_API_KEY")),
             QStringLiteral("environment-key"));
    QCOMPARE(environment.value(QStringLiteral("OPENROUTER_API_KEY")),
             QStringLiteral("wallet-router"));
    QCOMPARE(environment.value(QStringLiteral("UNRELATED")), QStringLiteral("value"));

    adapter.setCredentialOverrides({});
    QVERIFY(adapter.environmentWithCredentialOverrides({}).isEmpty());
}

QTEST_GUILESS_MAIN(CredentialStoreTest)

#include "tst_credential_store.moc"
