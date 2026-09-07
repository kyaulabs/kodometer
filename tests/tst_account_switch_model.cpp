#include <QJsonDocument>
#include <QtTest>
#include <kodometer/account_switch_model.hpp>
#include <kodometer/credential_store.hpp>
#include <kodometer/oauth_profiles.hpp>
#include <kodometer/wallet_accounts.hpp>

using namespace Kodometer;

class SwitchBackend final : public CredentialBackend
{
  public:
    QMap<QString, QString> records;
    int writes = 0;
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
        return records;
    }
    bool writeSecret(const QString &key, const QString &value, QString *) override
    {
        ++writes;
        records[key] = value;
        emit changed();
        return true;
    }
    bool removeSecret(const QString &key, QString *) override
    {
        ++writes;
        records.remove(key);
        emit changed();
        return true;
    }
    void close()
    {
        emit closed();
    }
};

QVariantMap view(const AccountSwitchModel &model, const QString &provider)
{
    for (const QVariant &row : model.providers())
        if (row.toMap().value("id") == provider)
            return row.toMap();
    return {};
}

class AccountSwitchModelTest final : public QObject
{
    Q_OBJECT
  private slots:
    void preparesOnlyNonsecretProfileSelections()
    {
        AccountSwitchModel model;
        QCOMPARE(model.providers().size(), 8);
        QVERIFY(!model.accounts());
        OAuthProfiles profiles;
        for (const QString &provider :
             {QStringLiteral("codex"), QStringLiteral("claude"), QStringLiteral("gemini")}) {
            QVERIFY(profiles.addProfile(provider, "<b>Work</b>", "/profiles/work"));
            const QString id = profiles.selectedId(provider);
            const QString original = profiles.configuration();
            QVERIFY(model.setProperty("oauthConfiguration", original));
            const auto row = view(model, provider);
            QCOMPARE(row.value("selectedId").toString(), id);
            QCOMPARE(row.value("choices").toList().size(), 2);
            QVERIFY(
                !QJsonDocument::fromVariant(model.providers()).toJson().contains("/profiles/work"));
            const auto change = model.selectionChange(provider, "default");
            QCOMPARE(change.value("key").toString(), QStringLiteral("oauthProfiles"));
            OAuthProfiles next;
            next.setConfiguration(change.value("value").toString());
            QCOMPARE(next.selectedId(provider), QStringLiteral("default"));
            QCOMPARE(next.entries(provider), profiles.entries(provider));
            QCOMPARE(model.property("oauthConfiguration").toString(), original);
            QCOMPARE(model.selectionChange(provider, id).value("value").toString(), original);
            QVERIFY(model.selectionChange(provider, "missing").isEmpty());
        }
        QVERIFY(model.selectionChange("unknown", "default").isEmpty());
        model.setProperty("disabledProviders", QStringList{"codex"});
        QVERIFY(view(model, "codex").isEmpty());
        QVERIFY(model.selectionChange("codex", "default").isEmpty());
        model.setProperty("oauthConfiguration", "invalid");
        QVERIFY(!view(model, "gemini").value("enabled").toBool());
        QVERIFY(model.selectionChange("gemini", "default").isEmpty());
        QCOMPARE(model.selectionChange("deepseek", "").value("value").toString(), QString{});
    }

    void selectsWalletMetadataWithoutMutatingEntries()
    {
        AccountSwitchModel model;
        auto *backend = new SwitchBackend;
        CredentialStore store(backend);
        store.open();
        model.setAccounts(store.accounts());
        model.setAccounts(store.accounts());
        QCOMPARE(model.accounts(), store.accounts());
        for (const QString &provider :
             {QStringLiteral("deepseek"), QStringLiteral("kimi"), QStringLiteral("openrouter"),
              QStringLiteral("xai"), QStringLiteral("zai")}) {
            const QString id = store.accounts()->addAccount(
                provider, "Work", "private-key", {},
                provider == "xai" ? QStringLiteral("private-team") : QString{},
                provider == "zai" ? QVariantMap{{"region", "global"},
                                                {"scope", "team"},
                                                {"organizationId", "private-org"},
                                                {"projectId", "private-project"}}
                                  : QVariantMap{});
            QVERIFY(!id.isEmpty());
            const int writes = backend->writes;
            const auto change = model.selectionChange(provider, id);
            QCOMPARE(change.value("key").toString(), provider + QStringLiteral("AccountId"));
            QCOMPARE(change.value("value").toString(), id);
            QCOMPARE(backend->writes, writes);
            const auto serialized = QJsonDocument::fromVariant(model.providers()).toJson();
            QVERIFY(!serialized.contains("private-"));
            model.setProperty("walletSelections", QVariantMap{{provider, id}});
            QCOMPARE(view(model, provider).value("selectedId").toString(), id);
            QVERIFY(store.accounts()->removeAccount(provider, id));
            const auto choices = view(model, provider).value("choices").toList();
            QCOMPARE(choices.size(), 2);
            QVERIFY(!choices.last().toMap().value("available").toBool());
            QVERIFY(model.selectionChange(provider, id).isEmpty());
            QCOMPARE(model.selectionChange(provider, "").value("value").toString(), QString{});
        }
        backend->close();
        QVERIFY(model.selectionChange("zai", "missing").isEmpty());
        QVERIFY(!model.selectionChange("zai", "").isEmpty());
    }

    void tracksWalletRebindingAndDestruction()
    {
        AccountSwitchModel model;
        QSignalSpy changes(&model, &AccountSwitchModel::changed);
        auto *backend = new SwitchBackend;
        auto *store = new CredentialStore(backend);
        store->open();
        model.setAccounts(store->accounts());
        const auto before = changes.count();
        QVERIFY(!store->accounts()->addAccount("kimi", "Work", "key").isEmpty());
        QVERIFY(changes.count() > before);
        model.setAccounts(nullptr);
        const auto detached = changes.count();
        backend->close();
        QCOMPARE(changes.count(), detached);
        model.setAccounts(store->accounts());
        delete store;
        QVERIFY(!model.accounts());
        QCOMPARE(view(model, "kimi").value("choices").toList().size(), 1);
    }
};

QTEST_GUILESS_MAIN(AccountSwitchModelTest)
#include "tst_account_switch_model.moc"
