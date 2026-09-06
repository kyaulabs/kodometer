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
        QCOMPARE(accounts->providers().size(), 4);
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
        QVERIFY(accounts->addAccount("zai", "Work", "key").isEmpty());
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
        for (int count : {2, 9, 33}) {
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

    void storesOpenRouterKeysAsOneAccount()
    {
        auto *backend = new AccountBackend;
        CredentialStore store(backend);
        store.open();
        auto *accounts = store.accounts();
        QVERIFY(WalletAccounts::isAccountEntry("accounts/openrouter/" + Id));
        const QString id = accounts->addAccount("openrouter", "Work", "ordinary", "management");
        QVERIFY(!id.isEmpty());
        QCOMPARE(backend->values.size(), 1);
        QCOMPARE(*accounts->key("openrouter", id), QStringLiteral("ordinary"));
        QCOMPARE(accounts->managementKey("openrouter", id), QStringLiteral("management"));
        QCOMPARE(accounts->metaObject()->indexOfMethod("managementKey(QString,QString)"), -1);
        QCOMPARE(accounts->entries("openrouter").first().toMap().size(), 2);
        QVERIFY(accounts->replaceAccount("openrouter", id, "replacement", ""));
        QVERIFY(accounts->managementKey("openrouter", id).isEmpty());
        QVERIFY(!accounts->replaceAccount("openrouter", id, "key", "bad\nkey"));
        QCOMPARE(*accounts->key("openrouter", id), QStringLiteral("replacement"));
        QVERIFY(accounts->addAccount("kimi", "Wrong", "key", "management").isEmpty());
        QVERIFY(accounts->addAccount("openrouter", "Wrong", "", "management").isEmpty());
        backend->close();
        QVERIFY(accounts->managementKey("openrouter", id).isEmpty());
        store.open();
        QCOMPARE(*accounts->key("openrouter", id), QStringLiteral("replacement"));
    }

    void boundsPairedKeysAndAcceptsAllFourProviders()
    {
        auto *backend = new AccountBackend;
        CredentialStore store(backend);
        store.open();
        auto *accounts = store.accounts();
        const QString maximum(65536, '"');
        const QString id = accounts->addAccount("openrouter", "Maximum", maximum, maximum);
        QVERIFY(!id.isEmpty());
        QCOMPARE(*accounts->key("openrouter", id), maximum);
        QCOMPARE(accounts->managementKey("openrouter", id), maximum);
        QVERIFY(backend->values.first().toUtf8().size() > 256 * 1024);
        QVERIFY(!accounts->replaceAccount("openrouter", id, "key", QString(65537, 'x')));
        QVERIFY(!accounts->replaceAccount("openrouter", id, "key", QStringLiteral("bad\u00e9key")));
        QVERIFY(accounts->removeAccount("openrouter", id));
        for (const QString &provider : {QStringLiteral("deepseek"), QStringLiteral("kimi"),
                                        QStringLiteral("openrouter"), QStringLiteral("xai")}) {
            for (int i = 0; i < 8; ++i) {
                const QString uuid =
                    QStringLiteral("11111111-1111-4111-8111-%1").arg(i, 12, 10, QLatin1Char('0'));
                QJsonObject record{{"name", QString::number(i)}, {"key", "key"}};
                if (provider == QLatin1String("xai"))
                    record.insert("teamId", "Team_012");
                backend->values.insert(
                    "accounts/" + provider + "/" + uuid,
                    QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Compact)));
            }
        }
        backend->update();
        QVERIFY(accounts->ready());
        QCOMPARE(backend->values.size(), 32);
        QCOMPARE(accounts->entries("xai").size(), 8);
        QVERIFY(accounts->addAccount("xai", "Nine", "key", {}, "team").isEmpty());
        QCOMPARE(accounts->entries("openrouter").size(), 8);
        QVERIFY(accounts->addAccount("openrouter", "Nine", "key").isEmpty());
    }

    void storesZaiSelectorsTogether()
    {
        auto *backend = new AccountBackend;
        CredentialStore store(backend);
        store.open();
        auto *accounts = store.accounts();
        const QVariantMap personal{{"region", "global"}, {"scope", "personal"}};
        const QVariantMap team{{"region", "bigmodel-cn"},
                               {"scope", "team"},
                               {"organizationId", "org-1"},
                               {"projectId", "project_2"}};
        QVERIFY(WalletAccounts::isAccountEntry("accounts/zai/" + Id));
        const QString id = accounts->addAccount("zai", "Work", "key", {}, {}, team);
        QVERIFY(!id.isEmpty());
        QCOMPARE(accounts->zaiOptions("zai", id), team);
        QCOMPARE(accounts->entries("zai").first().toMap().size(), 2);
        QCOMPARE(accounts->metaObject()->indexOfMethod("zaiOptions(QString,QString)"), -1);
        QVERIFY(accounts->replaceAccount("zai", id, "replacement", {}, {}, personal));
        QCOMPARE(accounts->zaiOptions("zai", id), personal);
        QVERIFY(accounts->addAccount("deepseek", "Wrong", "key", {}, {}, personal).isEmpty());
        QVERIFY(accounts->addAccount("zai", "Wrong", "key", "management", {}, team).isEmpty());
        QVERIFY(accounts->addAccount("zai", "Wrong", "key", {}, "team", team).isEmpty());
        for (const QString &field : team.keys()) {
            for (const QVariant &bad :
                 {QVariant{}, QVariant(true), QVariant(7), QVariant(""), QVariant("../bad"),
                  QVariant("%2f"), QVariant("bad\nvalue"), QVariant(QString(257, 'a'))}) {
                QVariantMap invalid = team;
                invalid[field] = bad;
                QVERIFY(!accounts->replaceAccount("zai", id, "key", {}, {}, invalid));
                QCOMPARE(accounts->zaiOptions("zai", id), personal);
            }
            QVariantMap missing = team;
            missing.remove(field);
            QVERIFY(!accounts->replaceAccount("zai", id, "key", {}, {}, missing));
        }
        QVariantMap extra = personal;
        extra["organizationId"] = "org";
        QVERIFY(!accounts->replaceAccount("zai", id, "key", {}, {}, extra));
        backend->close();
        QVERIFY(accounts->zaiOptions("zai", id).isEmpty());
        store.open();
        QCOMPARE(accounts->zaiOptions("zai", id), personal);
        QVERIFY(accounts->removeAccount("zai", id));
    }

    void storesXaiKeyAndTeamTogether()
    {
        auto *backend = new AccountBackend;
        CredentialStore store(backend);
        store.open();
        auto *accounts = store.accounts();
        QVERIFY(WalletAccounts::isAccountEntry("accounts/xai/" + Id));
        const QString id = accounts->addAccount("xai", "Work", "management-key", {}, " team-123 ");
        QVERIFY(!id.isEmpty());
        QCOMPARE(backend->values.size(), 1);
        QCOMPARE(*accounts->key("xai", id), QStringLiteral("management-key"));
        QCOMPARE(accounts->teamId("xai", id), QStringLiteral("team-123"));
        QCOMPARE(accounts->metaObject()->indexOfMethod("teamId(QString,QString)"), -1);
        QCOMPARE(accounts->entries("xai").first().toMap().size(), 2);
        QVERIFY(accounts->replaceAccount("xai", id, "replacement", {}, "team-456"));
        QCOMPARE(accounts->teamId("xai", id), QStringLiteral("team-456"));
        for (const QString &team :
             {QString{}, QStringLiteral(".."), QStringLiteral("a/b"), QStringLiteral("a\\b"),
              QStringLiteral("%2f"), QStringLiteral("bad\nteam"), QStringLiteral("bad team"),
              QStringLiteral("bad\u00e9team"), QString(257, 'x')}) {
            QVERIFY(!accounts->replaceAccount("xai", id, "key", {}, team));
            QCOMPARE(accounts->teamId("xai", id), QStringLiteral("team-456"));
        }
        const QString maximumTeam(256, 'A');
        QVERIFY(accounts->replaceAccount("xai", id, "key", {}, maximumTeam));
        QCOMPARE(accounts->teamId("xai", id), maximumTeam);
        QVERIFY(accounts->replaceAccount("xai", id, "replacement", {}, "team-456"));
        QVERIFY(accounts->addAccount("xai", "Wrong", "key", "extra-management", "team").isEmpty());
        QVERIFY(accounts->addAccount("deepseek", "Wrong", "key", {}, "team").isEmpty());
        QVERIFY(accounts->addAccount("xai", "Missing", "key").isEmpty());
        backend->close();
        QVERIFY(accounts->teamId("xai", id).isEmpty());
        store.open();
        QCOMPARE(accounts->teamId("xai", id), QStringLiteral("team-456"));
        QVERIFY(accounts->removeAccount("xai", id));
        QVERIFY(accounts->teamId("xai", id).isEmpty());
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
        const QString xai = "accounts/xai/" + Id;
        QTest::newRow("xai-no-team") << xai << entry();
        QTest::newRow("xai-team-type")
            << xai << QStringLiteral(R"({"name":"Work","key":"key","teamId":2})");
        QTest::newRow("xai-empty-team")
            << xai << QStringLiteral(R"({"name":"Work","key":"key","teamId":""})");
        QTest::newRow("xai-invalid-team")
            << xai << QStringLiteral(R"({"name":"Work","key":"key","teamId":"../other"})");
        QTest::newRow("xai-extra")
            << xai
            << QStringLiteral(
                   R"({"name":"Work","key":"key","teamId":"team","managementKey":"extra"})");
        QTest::newRow("unsupported-team")
            << key << QStringLiteral(R"({"name":"Work","key":"key","teamId":"team"})");
        const QString router = "accounts/openrouter/" + Id;
        QTest::newRow("router-large") << router << QString(384 * 1024 + 1, ' ');
        QTest::newRow("management-type")
            << router << QStringLiteral(R"({"name":"Work","key":"key","managementKey":2})");
        QTest::newRow("management-invalid")
            << router
            << QStringLiteral(R"({"name":"Work","key":"key","managementKey":"bad\nkey"})");
        QTest::newRow("management-unsupported")
            << key << QStringLiteral(R"({"name":"Work","key":"key","managementKey":"management"})");
        QTest::newRow("router-extra")
            << router << QStringLiteral(R"({"name":"Work","key":"key","unexpected":true})");
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
