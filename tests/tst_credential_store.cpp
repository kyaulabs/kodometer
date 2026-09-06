#include <kodometer/credential_store.hpp>
#include <kodometer/provider_adapter.hpp>

#include <QtTest>

#include <functional>
#include <utility>

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
        const auto snapshot = values;
        const bool failed = readFails;
        const QString failure = readError;
        auto callback = std::exchange(duringRead, {});
        if (callback)
            callback();
        if (failed || !failure.isEmpty()) {
            if (error != nullptr) {
                *error = failure;
            }
            return std::nullopt;
        }
        QMap<QString, QString> result;
        for (const QString &key : keys) {
            if (snapshot.contains(key)) {
                result.insert(key, snapshot.value(key));
            }
        }
        return result;
    }

    std::optional<QMap<QString, QString>> readAccountEntries(QString *) override
    {
        auto callback = std::exchange(duringAccountRead, {});
        if (callback)
            callback();
        return accountValues;
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

    std::function<void()> duringRead;
    std::function<void()> duringAccountRead;
    QMap<QString, QString> values;
    QMap<QString, QString> accountValues;
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
    void retriesDefaultCredentialsWhileAlreadyOpen();
    void invalidatesFailedReadsAndRecovers_data();
    void invalidatesFailedReadsAndRecovers();
    void discardsReentrantReads_data();
    void discardsReentrantReads();
    void ignoresLateOpenAfterClose();
    void closeDuringCompletionNotificationWins();
    void closeDuringMutation_data();
    void closeDuringMutation();
    void abandonsOpenFromNotifications_data();
    void abandonsOpenFromNotifications();
    void retainsNamedAccountsWhenDefaultReadFails();
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

void CredentialStoreTest::retriesDefaultCredentialsWhileAlreadyOpen()
{
    auto *backend = new FakeCredentialBackend;
    CredentialStore store(backend);
    store.open();
    backend->finishOpen(true);
    backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("recovered"));
    store.open(); // Manual retry must not depend on a folderUpdated notification.
    QCOMPARE(store.secrets().value(QStringLiteral("DEEPSEEK_API_KEY")),
             QStringLiteral("recovered"));
    QCOMPARE(backend->openCount, 1);
    backend->duringAccountRead = [&] { backend->close(); };
    const int reads = backend->readCount;
    store.open();
    QCOMPARE(backend->readCount, reads);
    QVERIFY(!store.ready());
    QVERIFY(store.secrets().isEmpty());
}

void CredentialStoreTest::invalidatesFailedReadsAndRecovers_data()
{
    QTest::addColumn<bool>("invalid");
    QTest::newRow("read-error") << false;
    QTest::newRow("malformed-entry") << true;
}

void CredentialStoreTest::invalidatesFailedReadsAndRecovers()
{
    QFETCH(bool, invalid);
    auto *backend = new FakeCredentialBackend;
    backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("old"));
    CredentialStore store(backend);
    store.open();
    backend->finishOpen(true);
    backend->readFails = !invalid;
    if (invalid)
        backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("bad\nkey"));
    backend->notifyChanged();
    QVERIFY(!store.ready());
    QVERIFY(store.secrets().isEmpty());
    QVERIFY(store.configuredKeys().isEmpty());
    QVERIFY(!store.error().isEmpty());
    backend->readFails = false;
    backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("repaired"));
    backend->notifyChanged();
    QVERIFY(store.ready());
    QVERIFY(store.error().isEmpty());
    QCOMPARE(store.secrets().value(QStringLiteral("DEEPSEEK_API_KEY")), QStringLiteral("repaired"));
    QCOMPARE(backend->openCount, 1);
}

void CredentialStoreTest::discardsReentrantReads_data()
{
    QTest::addColumn<bool>("opening");
    QTest::addColumn<bool>("closing");
    QTest::addColumn<bool>("failed");
    for (bool opening : {false, true})
        for (bool closing : {false, true})
            for (bool failed : {false, true})
                QTest::newRow(
                    qPrintable(QStringLiteral("%1-%2-%3").arg(opening).arg(closing).arg(failed)))
                    << opening << closing << failed;
}

void CredentialStoreTest::discardsReentrantReads()
{
    QFETCH(bool, opening);
    QFETCH(bool, closing);
    QFETCH(bool, failed);
    auto *backend = new FakeCredentialBackend;
    backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("obsolete"));
    CredentialStore store(backend);
    store.open();
    if (!opening)
        backend->finishOpen(true);
    backend->readFails = failed;
    backend->duringRead = [&] {
        backend->readFails = false;
        if (closing) {
            backend->close();
        }
        else {
            backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("latest"));
            backend->notifyChanged();
        }
    };
    if (opening)
        backend->finishOpen(true);
    else
        backend->notifyChanged();
    QCOMPARE(store.ready(), !closing);
    QVERIFY(!store.busy());
    if (closing) {
        QVERIFY(store.secrets().isEmpty());
        QCOMPARE(store.error(), QStringLiteral("KWallet was closed"));
    }
    else {
        QCOMPARE(store.secrets().value(QStringLiteral("DEEPSEEK_API_KEY")),
                 QStringLiteral("latest"));
        QVERIFY(store.error().isEmpty());
    }
}

void CredentialStoreTest::ignoresLateOpenAfterClose()
{
    auto *backend = new FakeCredentialBackend;
    backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("obsolete"));
    CredentialStore store(backend);
    store.open();
    backend->close();
    backend->finishOpen(true);
    QVERIFY(!store.ready());
    QVERIFY(!store.busy());
    QVERIFY(store.secrets().isEmpty());
    QCOMPARE(store.error(), QStringLiteral("KWallet was closed"));
    store.open();
    backend->finishOpen(true);
    QVERIFY(store.ready());
}

void CredentialStoreTest::closeDuringCompletionNotificationWins()
{
    auto *backend = new FakeCredentialBackend;
    backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("obsolete"));
    CredentialStore store(backend);
    store.open();
    connect(&store, &CredentialStore::busyChanged, &store, [&] {
        if (!store.busy())
            backend->close();
    });
    backend->finishOpen(true);
    QVERIFY(!store.ready());
    QVERIFY(store.secrets().isEmpty());
    QCOMPARE(store.error(), QStringLiteral("KWallet was closed"));
}

void CredentialStoreTest::closeDuringMutation_data()
{
    QTest::addColumn<bool>("remove");
    QTest::newRow("save") << false;
    QTest::newRow("remove") << true;
}

void CredentialStoreTest::closeDuringMutation()
{
    QFETCH(bool, remove);
    auto *backend = new FakeCredentialBackend;
    backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("old"));
    CredentialStore store(backend);
    store.open();
    backend->finishOpen(true);
    backend->duringRead = [&] { backend->close(); };
    const bool success =
        remove ? store.removeSecret(QStringLiteral("DEEPSEEK_API_KEY"))
               : store.saveSecret(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("new"));
    QVERIFY(!success);
    QVERIFY(store.secrets().isEmpty());
    QVERIFY(!store.ready());
    QCOMPARE(store.error(), QStringLiteral("KWallet was closed"));
}

void CredentialStoreTest::abandonsOpenFromNotifications_data()
{
    QTest::addColumn<QString>("notification");
    for (const auto &name : {QStringLiteral("busy"), QStringLiteral("error"),
                             QStringLiteral("accounts"), QStringLiteral("secrets")})
        QTest::newRow(qPrintable(name)) << name;
}

void CredentialStoreTest::abandonsOpenFromNotifications()
{
    QFETCH(QString, notification);
    auto *backend = new FakeCredentialBackend;
    backend->values.insert(QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("old"));
    CredentialStore store(backend);
    bool fired = false;
    auto closeOnce = [&] {
        if (!std::exchange(fired, true))
            backend->close();
    };
    if (notification == QLatin1String("busy"))
        connect(&store, &CredentialStore::busyChanged, &store, closeOnce);
    else if (notification == QLatin1String("error")) {
        backend->close();
        connect(&store, &CredentialStore::errorChanged, &store, closeOnce);
    }
    else if (notification == QLatin1String("accounts"))
        backend->duringAccountRead = closeOnce;
    else
        connect(&store, &CredentialStore::secretsChanged, &store, closeOnce);
    store.open();
    backend->finishOpen(true);
    QVERIFY(fired);
    QVERIFY(!store.ready());
    QVERIFY(!store.busy());
    QVERIFY(store.secrets().isEmpty());
    QVERIFY(!store.accounts()->ready());
    QCOMPARE(store.error(), QStringLiteral("KWallet was closed"));
}

void CredentialStoreTest::retainsNamedAccountsWhenDefaultReadFails()
{
    auto *backend = new FakeCredentialBackend;
    const QString id = QStringLiteral("11111111-1111-4111-8111-111111111111");
    backend->accountValues.insert(QStringLiteral("accounts/deepseek/") + id,
                                  QStringLiteral(R"({"name":"Work","key":"named-key"})"));
    CredentialStore store(backend);
    store.open();
    backend->finishOpen(true);
    backend->readFails = true;
    backend->notifyChanged();
    QVERIFY(!store.ready());
    QVERIFY(store.accounts()->ready());
    QCOMPARE(store.accounts()->key(QStringLiteral("deepseek"), id).value(),
             QStringLiteral("named-key"));
    backend->close();
    QVERIFY(!store.accounts()->ready());
    QVERIFY(!store.accounts()->key(QStringLiteral("deepseek"), id));
}

QTEST_GUILESS_MAIN(CredentialStoreTest)

#include "tst_credential_store.moc"
