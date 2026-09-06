#include <kodometer/credential_store.hpp>
#include <kodometer/wallet_accounts.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <functional>
#include <utility>

using Kodometer::CredentialBackend;
using Kodometer::CredentialStore;
using Kodometer::WalletAccounts;

class AccountBackend final : public CredentialBackend
{
  public:
    void open() override
    {
        emit openFinished(true, {});
    }
    std::optional<QMap<QString, QString>> readSecrets(const QStringList &, QString *) override
    {
        return QMap<QString, QString>{};
    }
    std::optional<QMap<QString, QString>> readAccountEntries(QString *) override
    {
        if (readFails)
            return std::nullopt;
        const auto snapshot = values;
        if (duringRead) {
            const auto callback = std::exchange(duringRead, {});
            callback();
        }
        return snapshot;
    }
    bool writeSecret(const QString &key, const QString &value, QString *) override
    {
        if (writeFails)
            return false;
        values.insert(key, value);
        emit changed();
        return true;
    }
    bool removeSecret(const QString &key, QString *) override
    {
        if (writeFails)
            return false;
        values.remove(key);
        emit changed();
        return true;
    }
    void close()
    {
        emit closed();
    }
    void update()
    {
        emit changed();
    }
    QMap<QString, QString> values;
    std::function<void()> duringRead;
    bool readFails = false;
    bool writeFails = false;
};

namespace {
QString entry(const QString &name = QStringLiteral("Work"),
              const QString &key = QStringLiteral("private-key"))
{
    return QString::fromUtf8(
        QJsonDocument(QJsonObject{{"name", name}, {"key", key}}).toJson(QJsonDocument::Compact));
}
const QString Id = QStringLiteral("11111111-1111-4111-8111-111111111111");
} // namespace

class WalletAccountsTest final : public QObject
{
    Q_OBJECT
  private slots:
    void managesIndependentWalletEntriesWithoutExposingKeys()
    {
        auto *backend = new AccountBackend;
        CredentialStore store(backend);
        auto *accounts = store.accounts();
        QVERIFY(!accounts->ready());
        QVERIFY(!accounts->reload());
        QCOMPARE(accounts->metaObject()->indexOfMethod("key(QString,QString)"), -1);
        QVERIFY(!accounts->property("key").isValid());
        QVERIFY(WalletAccounts::isAccountEntry("accounts/deepseek/" + Id));
        QVERIFY(WalletAccounts::isAccountEntry("accounts/kimi/" + Id));
        QVERIFY(!WalletAccounts::isAccountEntry("DEEPSEEK_API_KEY"));
        QVERIFY(!accounts->key("deepseek", Id));
        QVERIFY(accounts->addAccount("deepseek", "Work", "secret").isEmpty());
        store.open();
        QVERIFY(accounts->ready());
        QVERIFY(accounts->error().isEmpty());
        QCOMPARE(accounts->providers().size(), 2);
        QSignalSpy changed(accounts, &WalletAccounts::changed);
        const QString id = accounts->addAccount("deepseek", " Work ", " key-one ");
        QVERIFY(!id.isEmpty());
        QCOMPARE(backend->values.size(), 1);
        QVERIFY(backend->values.contains("accounts/deepseek/" + id));
        QCOMPARE(accounts->entries("deepseek").size(), 1);
        QCOMPARE(accounts->name("deepseek", id), QStringLiteral("Work"));
        QCOMPARE(*accounts->key("deepseek", id), QStringLiteral("key-one"));
        QVERIFY(!accounts->key("kimi", id));
        QVERIFY(!accounts->key("deepseek", "invalid"));
        QVERIFY(accounts->entries("unknown").isEmpty());
        QVERIFY(!accounts->providers().value("deepseek").toList().first().toMap().contains("key"));
        QVERIFY(store.secrets().isEmpty());
        QVERIFY(store.configuredKeys().isEmpty());
        QVERIFY(!store.hasSecret("accounts/deepseek/" + id));
        QVERIFY(!store.saveSecret("accounts/deepseek/" + id, "not allowed"));
        QVERIFY(!store.removeSecret("accounts/deepseek/" + id));
        const QString second = accounts->addAccount("kimi", "Personal", "key-two");
        QVERIFY(!second.isEmpty());
        QVERIFY(accounts->replaceAccount("deepseek", id, "replacement"));
        QCOMPARE(*accounts->key("deepseek", id), QStringLiteral("replacement"));
        QCOMPARE(*accounts->key("kimi", second), QStringLiteral("key-two"));
        const qsizetype count = changed.count();
        backend->update();
        QCOMPARE(changed.count(), count);
        QVERIFY(accounts->removeAccount("deepseek", id));
        QVERIFY(!accounts->key("deepseek", id));
        QCOMPARE(backend->values.size(), 1);
        QCOMPARE(*accounts->key("kimi", second), QStringLiteral("key-two"));
        backend->close();
        QVERIFY(!accounts->ready());
        QVERIFY(accounts->entries("kimi").isEmpty());
        QVERIFY(!accounts->key("kimi", second));
        store.open();
        QVERIFY(accounts->ready());
        QCOMPARE(*accounts->key("kimi", second), QStringLiteral("key-two"));
    }

    void validatesEditsAndBoundsAccountCounts()
    {
        auto *backend = new AccountBackend;
        CredentialStore store(backend);
        store.open();
        auto *accounts = store.accounts();
        QVERIFY(accounts->addAccount("openrouter", "Work", "key").isEmpty());
        for (const auto &pair : QList<QPair<QString, QString>>{{"", "key"},
                                                               {QString(65, 'a'), "key"},
                                                               {"Bad\nName", "key"},
                                                               {"Work", ""},
                                                               {"Work", "bad\nkey"},
                                                               {"Work", "bad\rkey"},
                                                               {"Work", QString(65537, 'x')}}) {
            QVERIFY(accounts->addAccount("deepseek", pair.first, pair.second).isEmpty());
            QVERIFY(!accounts->error().isEmpty());
            QVERIFY(backend->values.isEmpty());
        }
        const QString id = accounts->addAccount("deepseek", "Work", "key");
        QVERIFY(!id.isEmpty());
        QVERIFY(accounts->addAccount("deepseek", "Work", "another").isEmpty());
        QVERIFY(!accounts->replaceAccount("deepseek", id, ""));
        QVERIFY(!accounts->replaceAccount("kimi", id, "other"));
        QVERIFY(!accounts->removeAccount("deepseek", "missing"));
        for (int i = 1; i < 8; ++i) {
            QVERIFY(!accounts->addAccount("deepseek", QString::number(i), "key").isEmpty());
        }
        QVERIFY(accounts->addAccount("deepseek", "Nine", "key").isEmpty());
        QCOMPARE(accounts->entries("deepseek").size(), 8);
        QVERIFY(!accounts->addAccount("kimi", "Work", "key").isEmpty());
    }

    void failsClosedOnReadFailureAndReportsWriteFailure()
    {
        auto *backend = new AccountBackend;
        backend->values.insert("accounts/deepseek/" + Id, entry());
        CredentialStore store(backend);
        store.open();
        auto *accounts = store.accounts();
        QVERIFY(accounts->key("deepseek", Id));
        backend->writeFails = true;
        QVERIFY(accounts->addAccount("deepseek", "New", "key").isEmpty());
        QVERIFY(!accounts->replaceAccount("deepseek", Id, "key"));
        QVERIFY(!accounts->removeAccount("deepseek", Id));
        QVERIFY(!accounts->error().isEmpty());
        QCOMPARE(*accounts->key("deepseek", Id), QStringLiteral("private-key"));
        backend->readFails = true;
        backend->update();
        QVERIFY(!accounts->ready());
        QVERIFY(!accounts->key("deepseek", Id));
        QVERIFY(accounts->entries("deepseek").isEmpty());
        QVERIFY(!accounts->replaceAccount("deepseek", Id, "key"));
        QVERIFY(!accounts->removeAccount("deepseek", Id));
        backend->readFails = false;
        backend->update();
        QVERIFY(accounts->ready());
        QVERIFY(accounts->key("deepseek", Id));
    }

    void preventsReentrantReadsFromRestoringStaleKeys()
    {
        auto *backend = new AccountBackend;
        backend->values.insert("accounts/deepseek/" + Id, entry());
        CredentialStore store(backend);
        store.open();
        auto *accounts = store.accounts();
        backend->duringRead = [backend] { backend->close(); };
        backend->update();
        QVERIFY(!accounts->ready());
        QVERIFY(!accounts->key("deepseek", Id));
        store.open();
        backend->duringRead = [backend] {
            backend->values["accounts/deepseek/" + Id] = entry("New name", "new-key");
            backend->update();
        };
        backend->update();
        QCOMPARE(*accounts->key("deepseek", Id), QStringLiteral("new-key"));
        QCOMPARE(accounts->name("deepseek", Id), QStringLiteral("New name"));
    }

    void rejectsDuplicateNamesAndOversizedRegistries()
    {
        auto *backend = new AccountBackend;
        CredentialStore store(backend);
        store.open();
        for (int count : {2, 9, 17}) {
            backend->values.clear();
            for (int i = 0; i < count; ++i) {
                const QString id =
                    QStringLiteral("11111111-1111-4111-8111-%1").arg(i, 12, 10, QLatin1Char('0'));
                backend->values.insert(
                    "accounts/deepseek/" + id,
                    entry(count == 2 ? QStringLiteral("Duplicate") : QString::number(i)));
            }
            backend->update();
            QVERIFY(!store.accounts()->ready());
            QVERIFY(store.accounts()->entries("deepseek").isEmpty());
        }
        backend->values.clear();
        store.open();
        QVERIFY(store.accounts()->ready());
    }

    void rejectsMalformedStoredAccounts_data()
    {
        QTest::addColumn<QString>("walletKey");
        QTest::addColumn<QString>("payload");
        const QString key = "accounts/deepseek/" + Id;
        QTest::newRow("bad-json") << key << QStringLiteral("{");
        QTest::newRow("array") << key << QStringLiteral("[]");
        QTest::newRow("missing-fields") << key << QStringLiteral("{}");
        QTest::newRow("extra-fields")
            << key << QStringLiteral(R"({"name":"Work","key":"key","extra":true})");
        QTest::newRow("bad-type") << key << QStringLiteral(R"({"name":2,"key":"key"})");
        QTest::newRow("bad-key-type") << key << QStringLiteral(R"({"name":"Work","key":2})");
        QTest::newRow("empty-key") << key << entry("Work", "");
        QTest::newRow("bad-name") << key << entry("Bad\nName");
        QTest::newRow("large") << key << QString(262145, ' ');
        QTest::newRow("bad-id") << QStringLiteral("accounts/deepseek/default") << entry();
        QTest::newRow("wrong-provider") << ("accounts/other/" + Id) << entry();
        QTest::newRow("wrong-prefix") << ("elsewhere/deepseek/" + Id) << entry();
        QTest::newRow("missing-parts") << QStringLiteral("accounts/deepseek") << entry();
        QTest::newRow("noncanonical-id") << ("accounts/deepseek/{" + Id + "}") << entry();
        QTest::newRow("format-name") << key << entry(QStringLiteral("Bad\u200bName"));
        QTest::newRow("unicode-key") << key << entry("Work", QStringLiteral("bad\u00e9key"));
        QTest::newRow("control-key") << key << entry("Work", QStringLiteral("bad\tkey"));
    }

    void rejectsMalformedStoredAccounts()
    {
        QFETCH(QString, walletKey);
        QFETCH(QString, payload);
        auto *backend = new AccountBackend;
        backend->values.insert(walletKey, payload);
        CredentialStore store(backend);
        store.open();
        QVERIFY(!store.accounts()->ready());
        QVERIFY(!store.accounts()->error().isEmpty());
        QVERIFY(!store.accounts()->key("deepseek", Id));
        QVERIFY(store.ready()); // Account errors do not hide valid Default wallet entries.
    }
};

QTEST_GUILESS_MAIN(WalletAccountsTest)
#include "tst_wallet_accounts.moc"
