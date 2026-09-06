#include <QDesktopServices>
#include <QFile>
#include <QLibrary>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <KConfig>
#include <KConfigGroup>
#include <KConfigLoader>

namespace {

QQuickItem *findVisualItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem *child : root->childItems()) {
        if (auto *found = findVisualItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}

} // namespace

class AccountUiStore final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready MEMBER ready NOTIFY changed)
    Q_PROPERTY(QString error MEMBER error NOTIFY changed)
    Q_PROPERTY(QVariantMap providers READ providers NOTIFY changed)
  public:
    bool ready = true;
    QString error;
    bool failWrites = false;
    QString savedKey;
    QString removedId;
    const QString id = QStringLiteral("11111111-1111-4111-8111-111111111111");
    QVariantMap records;
    QVariantMap providers() const
    {
        return records;
    }
    Q_INVOKABLE QString addAccount(const QString &provider, const QString &name, const QString &key)
    {
        if (failWrites)
            return {};
        savedKey = key;
        records.insert(provider, QVariantList{QVariantMap{{"id", id}, {"name", name}}});
        emit changed();
        return id;
    }
    Q_INVOKABLE bool replaceAccount(const QString &, const QString &, const QString &key)
    {
        if (failWrites)
            return false;
        savedKey = key;
        return true;
    }
    Q_INVOKABLE bool removeAccount(const QString &provider, const QString &selected)
    {
        removedId = selected;
        records.remove(provider);
        emit changed();
        return true;
    }
  signals:
    void changed();
};

class AppletConfigurationTest final : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase()
    {
        QVERIFY2(m_plugin.load(), qPrintable(m_plugin.errorString()));
    }

    void exposesConfigurationAtResourceRoot()
    {
        QQmlEngine engine;
        const QUrl root(QStringLiteral("qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/"));
        QFile schema(QStringLiteral(":/qt/qml/plasma/applet/org/kyaulabs/kodometer/main.xml"));
        QVERIFY(schema.exists());
        QTemporaryDir temporary;
        KConfig config(temporary.filePath(QStringLiteral("settings")), KConfig::SimpleConfig);
        KConfigLoader loader(KConfigGroup(&config, "General"), &schema);
        QCOMPARE(loader.property("autoRefresh").toBool(), true);
        QCOMPARE(loader.property("refreshIntervalMinutes").toInt(), 5);
        QCOMPARE(loader.property("showIdleWindows").toBool(), false);
        QCOMPARE(loader.property("quotaNotifications").toBool(), false);
        QCOMPARE(loader.property("quotaNotificationThreshold").toInt(), 10);
        QCOMPARE(loader.property("oauthProfiles").toString(), QStringLiteral("{}"));
        QVERIFY(loader.property("disabledProviders").toStringList().isEmpty());

        QQmlComponent model(&engine, root.resolved(QUrl(QStringLiteral("config.qml"))));
        QScopedPointer<QObject> object(model.create());
        QVERIFY2(object, qPrintable(model.errorString()));
        auto *categories = qobject_cast<QAbstractItemModel *>(object.data());
        QVERIFY(categories);
        QCOMPARE(categories->rowCount(), 4);
        QVERIFY(loader.property("deepseekAccountId").toString().isEmpty());
        QVERIFY(loader.property("kimiAccountId").toString().isEmpty());
        const auto roles = categories->roleNames();
        const int sourceRole = roles.key(QByteArray("source"), -1);
        QVERIFY(sourceRole >= 0);
        for (int i = 0; i < categories->rowCount(); ++i) {
            const QString source = categories->data(categories->index(i, 0), sourceRole).toString();
            QVERIFY(!source.isEmpty());
            const QString resource = QStringLiteral(":") + root.resolved(QUrl(source)).path();
            QVERIFY2(QFile::exists(resource), qPrintable(resource));
        }
    }

    void persistsOnlyNonsecretPreferences()
    {
        QTemporaryDir temporary;
        const QString path = temporary.filePath(QStringLiteral("settings"));
        QFile schema(QStringLiteral(":/qt/qml/plasma/applet/org/kyaulabs/kodometer/main.xml"));
        {
            KConfig config(path, KConfig::SimpleConfig);
            KConfigLoader loader(KConfigGroup(&config, "Widget"), &schema);
            QCOMPARE(loader.items().size(), 9);
            const QVariantMap preferences{
                {QStringLiteral("autoRefresh"), false},
                {QStringLiteral("refreshIntervalMinutes"), 15},
                {QStringLiteral("disabledProviders"), QStringList{QStringLiteral("codex")}},
                {QStringLiteral("showIdleWindows"), true},
                {QStringLiteral("quotaNotifications"), true},
                {QStringLiteral("quotaNotificationThreshold"), 20},
                {QStringLiteral("deepseekAccountId"),
                 QStringLiteral("11111111-1111-4111-8111-111111111111")},
                {QStringLiteral("kimiAccountId"),
                 QStringLiteral("22222222-2222-4222-8222-222222222222")},
                {QStringLiteral("oauthProfiles"),
                 QStringLiteral(
                     R"({"version":1,"codex":[{"id":"11111111-1111-4111-8111-111111111111","name":"Work","directory":"/profiles/work"}],"selectedCodex":"11111111-1111-4111-8111-111111111111"})")}};
            for (auto it = preferences.cbegin(); it != preferences.cend(); ++it) {
                auto *item = loader.findItemByName(it.key());
                QVERIFY(item);
                item->setProperty(it.value());
            }
            QVERIFY(loader.save());
        }
        schema.close();
        KConfig config(path, KConfig::SimpleConfig);
        KConfigLoader reloaded(KConfigGroup(&config, "Widget"), &schema);
        QCOMPARE(reloaded.property("autoRefresh").toBool(), false);
        QCOMPARE(reloaded.property("refreshIntervalMinutes").toInt(), 15);
        QCOMPARE(reloaded.property("disabledProviders").toStringList(),
                 QStringList{QStringLiteral("codex")});
        QCOMPARE(reloaded.property("showIdleWindows").toBool(), true);
        QCOMPARE(reloaded.property("quotaNotifications").toBool(), true);
        QCOMPARE(reloaded.property("quotaNotificationThreshold").toInt(), 20);
        QCOMPARE(reloaded.property("deepseekAccountId").toString(),
                 QStringLiteral("11111111-1111-4111-8111-111111111111"));
        QCOMPARE(reloaded.property("kimiAccountId").toString(),
                 QStringLiteral("22222222-2222-4222-8222-222222222222"));
        QVERIFY(reloaded.property("oauthProfiles")
                    .toString()
                    .contains(QStringLiteral("/profiles/work")));
    }

    void stagesProfileEditsUntilApplied()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QQmlEngine engine;
        QQmlComponent backendComponent(&engine);
        backendComponent.setData(R"(import plasma.applet.org.kyaulabs.kodometer as Private
            Private.UsageController { autoRefresh: false; profiles.configuration: "{}" })",
                                 QUrl());
        QScopedPointer<QObject> backend(backendComponent.create());
        QVERIFY2(backend, qPrintable(backendComponent.errorString()));
        auto *liveProfiles = backend->property("profiles").value<QObject *>();
        QVERIFY(liveProfiles);
        QQmlComponent pageComponent(
            &engine, QUrl(QStringLiteral(
                         "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ConfigProfiles.qml")));
        QScopedPointer<QObject> page(pageComponent.create());
        QVERIFY2(page, qPrintable(pageComponent.errorString()));
        QCOMPARE(page->property("cfg_oauthProfiles").toString(), QStringLiteral("{}"));
        auto *name = page->findChild<QObject *>(QStringLiteral("codex-profile-name"));
        auto *directory = page->findChild<QObject *>(QStringLiteral("codex-profile-directory"));
        auto *add = page->findChild<QObject *>(QStringLiteral("codex-profile-add"));
        QVERIFY(name);
        QVERIFY(directory);
        QVERIFY(add);
        QVERIFY(name->setProperty("text", QStringLiteral("Work")));
        QVERIFY(directory->setProperty("text", QStringLiteral("/profiles/work")));
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        const QString staged = page->property("cfg_oauthProfiles").toString();
        QVERIFY(staged.contains(QStringLiteral("/profiles/work")));
        QCOMPARE(liveProfiles->property("configuration").toString(), QStringLiteral("{}"));
        auto *selector = page->findChild<QObject *>(QStringLiteral("codex-profile-selector"));
        auto *remove = page->findChild<QObject *>(QStringLiteral("codex-profile-remove"));
        QVERIFY(selector);
        QVERIFY(remove);
        QCOMPARE(selector->property("count").toInt(), 2);
        QVERIFY(selector->setProperty("currentIndex", 0));
        QVERIFY(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 0)));
        QVERIFY(!remove->property("enabled").toBool());
        QVERIFY(selector->setProperty("currentIndex", 1));
        QVERIFY(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 1)));
        QVERIFY(remove->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(remove, "clicked"));
        QCOMPARE(selector->property("count").toInt(), 1);
        QVERIFY(page->setProperty("cfg_oauthProfiles", QStringLiteral("invalid")));
        QVERIFY(!selector->property("enabled").toBool());
        QVERIFY(!add->property("enabled").toBool());
        QVERIFY(page->setProperty("cfg_oauthProfiles", QStringLiteral("{}")));
        QVERIFY(selector->property("enabled").toBool());
        QCOMPARE(selector->property("count").toInt(), 1);
        page.reset(); // Cancel destroys the draft without changing the running backend.
        QCOMPARE(liveProfiles->property("configuration").toString(), QStringLiteral("{}"));
        QVERIFY(liveProfiles->setProperty("configuration", staged)); // Apply.
        QCOMPARE(liveProfiles->property("configuration").toString(), staged);
        QVERIFY(!backend->property("busy").toBool());
        QVERIFY(!backend->findChild<QTimer *>(QStringLiteral("refreshTimer"))->isActive());
    }

    void stagesWalletSelectionsButMutatesWalletImmediately()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QQmlEngine engine;
        AccountUiStore accounts;
        QQmlComponent backendComponent(&engine);
        backendComponent.setData(R"(import plasma.applet.org.kyaulabs.kodometer as Private
            Private.UsageController { autoRefresh: false })",
                                 QUrl());
        QScopedPointer<QObject> backend(backendComponent.create());
        QVERIFY2(backend, qPrintable(backendComponent.errorString()));
        QQmlComponent component(
            &engine,
            QUrl(QStringLiteral(
                "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ConfigWalletAccounts.qml")));
        QScopedPointer<QObject> page(component.createWithInitialProperties(
            {{"accountStore", QVariant::fromValue<QObject *>(&accounts)}, {"width", 340}}));
        QVERIFY2(page, qPrintable(component.errorString()));
        QCOMPARE(page->property("cfg_deepseekAccountId").toString(), QString{});
        QCOMPARE(page->property("cfg_kimiAccountId").toString(), QString{});
        auto *name = page->findChild<QObject *>(QStringLiteral("deepseek-wallet-name"));
        auto *key = page->findChild<QObject *>(QStringLiteral("deepseek-wallet-key"));
        auto *add = page->findChild<QObject *>(QStringLiteral("deepseek-wallet-add"));
        auto *replace = page->findChild<QObject *>(QStringLiteral("deepseek-wallet-replace"));
        auto *remove = page->findChild<QObject *>(QStringLiteral("deepseek-wallet-remove"));
        auto *selector = page->findChild<QObject *>(QStringLiteral("deepseek-wallet-selector"));
        QVERIFY(name && key && add && replace && remove && selector);
        QCOMPARE(key->property("echoMode").toInt(), 2); // TextInput.Password
        QCOMPARE(key->property("text").toString(), QString{});
        QVERIFY(!add->property("enabled").toBool());
        QVERIFY(name->setProperty("text", QStringLiteral("<b>Work</b>")));
        QVERIFY(key->setProperty("text", QStringLiteral("private-key")));
        accounts.failWrites = true;
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        QCOMPARE(key->property("text").toString(), QStringLiteral("private-key"));
        accounts.failWrites = false;
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        QCOMPARE(accounts.savedKey, QStringLiteral("private-key"));
        QCOMPARE(key->property("text").toString(), QString{});
        QCOMPARE(name->property("text").toString(), QString{});
        QCOMPARE(page->property("cfg_deepseekAccountId").toString(), accounts.id);
        QCOMPARE(backend->property("deepseekAccountId").toString(), QString{});
        QCOMPARE(selector->property("count").toInt(), 2);
        QCOMPARE(selector->property("currentIndex").toInt(), 1);
        QCOMPARE(selector->property("displayText").toString(), QStringLiteral("<b>Work</b>"));
        QVERIFY(key->setProperty("text", QStringLiteral("replacement")));
        QVERIFY(replace->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(replace, "clicked"));
        QCOMPARE(accounts.savedKey, QStringLiteral("replacement"));
        QCOMPARE(key->property("text").toString(), QString{});
        const QString staged = page->property("cfg_deepseekAccountId").toString();
        page.reset(); // Cancel leaves the saved wallet entry but does not select it in the widget.
        QCOMPARE(accounts.providers().size(), 1);
        QCOMPARE(backend->property("deepseekAccountId").toString(), QString{});
        QVERIFY(backend->setProperty("deepseekAccountId", staged)); // Apply.
        page.reset(component.createWithInitialProperties(
            {{"accountStore", QVariant::fromValue<QObject *>(&accounts)},
             {"cfg_deepseekAccountId", staged}}));
        QVERIFY2(page, qPrintable(component.errorString()));
        remove = page->findChild<QObject *>(QStringLiteral("deepseek-wallet-remove"));
        selector = page->findChild<QObject *>(QStringLiteral("deepseek-wallet-selector"));
        key = page->findChild<QObject *>(QStringLiteral("deepseek-wallet-key"));
        QVERIFY(remove && selector && key);
        QVERIFY(key->setProperty("text", QStringLiteral("draft")));
        QVERIFY(accounts.setProperty("ready", false));
        QCOMPARE(key->property("text").toString(), QString{});
        QVERIFY(!remove->property("enabled").toBool());
        QVERIFY(accounts.setProperty("ready", true));
        QVERIFY(QMetaObject::invokeMethod(remove, "clicked"));
        QCOMPARE(accounts.removedId, staged);
        QCOMPARE(page->property("cfg_deepseekAccountId").toString(), staged);
        QCOMPARE(selector->property("displayText").toString(),
                 QStringLiteral("Selected account unavailable"));
        QVERIFY(!remove->property("enabled").toBool());
        QVERIFY(selector->setProperty("currentIndex", 0));
        QVERIFY(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 0)));
        QCOMPARE(page->property("cfg_deepseekAccountId").toString(), QString{});
        QVERIFY(!backend->property("busy").toBool());
    }

    void presentsWalletNamesAsPlainText()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral(
                         "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ProviderDetails.qml")));
        const QVariantMap provider{{QStringLiteral("id"), QStringLiteral("kimi")},
                                   {QStringLiteral("accountName"), QStringLiteral("<b>Work</b>")}};
        QScopedPointer<QObject> object(
            component.createWithInitialProperties({{QStringLiteral("provider"), provider}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *label = object->findChild<QObject *>(QStringLiteral("selectedWalletAccount"));
        QVERIFY(label);
        QCOMPARE(label->property("text").toString(), QStringLiteral("Account: <b>Work</b>"));
        QCOMPARE(label->property("textFormat").toInt(), static_cast<int>(Qt::PlainText));
    }

    void propagatesIdleWindowPreference()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QQmlEngine engine;
        const QVariantMap provider{
            {QStringLiteral("id"), QStringLiteral("codex")},
            {QStringLiteral("windows"),
             QVariantList{QVariantMap{{QStringLiteral("kind"), QStringLiteral("session")},
                                      {QStringLiteral("remainingPercent"), 50}},
                          QVariantMap{{QStringLiteral("kind"), QStringLiteral("weekly")},
                                      {QStringLiteral("idle"), true},
                                      {QStringLiteral("remainingPercent"), 100}}}}};
        for (const QString &file :
             {QStringLiteral("ProviderSummary.qml"), QStringLiteral("ProviderDetails.qml")}) {
            QQmlComponent component(
                &engine,
                QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/") + file));
            QScopedPointer<QObject> object(
                component.createWithInitialProperties({{QStringLiteral("provider"), provider}}));
            QVERIFY2(object, qPrintable(component.errorString()));
            QCOMPARE(object->property("windows").value<QJSValue>().toVariant().toList().size(), 1);
            QVERIFY(object->setProperty("showIdleWindows", true));
            QCOMPARE(object->property("windows").value<QJSValue>().toVariant().toList().size(), 2);
        }
    }

    void presentsCostHistoryAndSupportsKeyboardSelection()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QQmlEngine engine;
        QQuickWindow window;
        window.resize(400, 700);
        QQmlComponent component(
            &engine, QUrl(QStringLiteral(
                         "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ProviderDetails.qml")));
        const QVariantMap cost{
            {QStringLiteral("balanceUSD"), 7.0},
            {QStringLiteral("currencyCode"), QStringLiteral("USD")},
            {QStringLiteral("historyEndDate"), QStringLiteral("2027-01-15")},
            {QStringLiteral("historyIncludesCurrentDay"), true},
            {QStringLiteral("daily"),
             QVariantList{QVariantMap{{QStringLiteral("label"), QStringLiteral("2027-01-14")},
                                      {QStringLiteral("value"), 0.0}},
                          QVariantMap{{QStringLiteral("label"), QStringLiteral("2027-01-15")},
                                      {QStringLiteral("value"), 2.0}}}}};
        QVariantMap provider{{QStringLiteral("id"), QStringLiteral("xai")},
                             {QStringLiteral("cost"), cost}};
        QScopedPointer<QObject> object(
            component.createWithInitialProperties({{QStringLiteral("provider"), provider},
                                                   {QStringLiteral("width"), 400},
                                                   {QStringLiteral("height"), 700}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(item);
        item->setParentItem(window.contentItem());
        window.show();
        auto *chart = object->findChild<QQuickItem *>(QStringLiteral("costHistory"));
        QVERIFY(chart);
        QVERIFY(chart->isVisible());
        QCOMPARE(chart->property("dayCount").toInt(), 30);
        auto *selector = chart->findChild<QObject *>(QStringLiteral("costHistoryRange"));
        QVERIFY(selector);
        QVERIFY(selector->setProperty("currentIndex", 0));
        QCOMPARE(chart->property("dayCount").toInt(), 7);
        auto *selection = chart->findChild<QObject *>(QStringLiteral("costHistorySelection"));
        QVERIFY(selection);
        QCOMPARE(selection->property("text").toString(), QStringLiteral("2027-01-15: $2.00"));
        QQuickItem *button = nullptr;
        QTRY_VERIFY(button = findVisualItem(chart, QStringLiteral("history-day-2027-01-14")));
        QCOMPARE(button->property("reported").toBool(), true);
        button->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Space);
        QCOMPARE(selection->property("text").toString(), QStringLiteral("2027-01-14: $0.00"));
        QTRY_VERIFY(button = findVisualItem(chart, QStringLiteral("history-day-2027-01-13")));
        QCOMPARE(button->property("reported").toBool(), false);
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QCOMPARE(selection->property("text").toString(),
                 QStringLiteral("2027-01-13: Not reported"));
        for (const QString &name :
             {QStringLiteral("costHistoryPartial"), QStringLiteral("costHistoryCurrentDay")}) {
            auto *note = chart->findChild<QQuickItem *>(name);
            QVERIFY(note);
            QVERIFY(note->isVisible());
        }
        auto *estimated = chart->findChild<QQuickItem *>(QStringLiteral("costHistoryEstimated"));
        QVERIFY(estimated);
        QVERIFY(!estimated->isVisible());
        QVariantMap estimatedCost = cost;
        estimatedCost.insert(QStringLiteral("historyEstimated"), true);
        provider.insert(QStringLiteral("cost"), estimatedCost);
        QVERIFY(object->setProperty("provider", provider));
        QVERIFY(estimated->isVisible());
        QVERIFY(object->setProperty("width", 200));
        QTRY_VERIFY(chart->width() <= 200);
        provider.insert(QStringLiteral("cost"), QVariantMap{{QStringLiteral("balanceUSD"), 7.0}});
        QVERIFY(object->setProperty("provider", provider));
        QVERIFY(!chart->isVisible());
        QCOMPARE(object->property("hasBalance").toBool(), true);
        QVariantMap malformed = cost;
        malformed.insert(QStringLiteral("daily"), QStringLiteral("invalid"));
        provider.insert(QStringLiteral("cost"), malformed);
        QVERIFY(object->setProperty("provider", provider));
        QVERIFY(!chart->isVisible());
        QCOMPARE(object->property("hasBalance").toBool(), true);
        provider.insert(QStringLiteral("cost"), cost);
        QVERIFY(object->setProperty("provider", provider));
        QVERIFY(chart->isVisible());
        QCOMPARE(chart->property("dayCount").toInt(), 7);
    }

    void providerButtonsUseTrustedDestinations()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QDesktopServices::setUrlHandler(QStringLiteral("https"), this, "recordUrl");
        const auto resetHandler =
            qScopeGuard([] { QDesktopServices::unsetUrlHandler(QStringLiteral("https")); });
        m_openedUrls.clear();
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral(
                         "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ProviderDetails.qml")));
        QVariantMap provider{
            {QStringLiteral("id"), QStringLiteral("codex")},
            {QStringLiteral("dashboardUrl"), QStringLiteral("https://evil.test/?token=secret")},
            {QStringLiteral("profileName"), QStringLiteral("<b>Work</b>")}};
        QScopedPointer<QObject> object(component.createWithInitialProperties(
            {{QStringLiteral("provider"), provider}, {QStringLiteral("width"), 400}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *profileLabel = object->findChild<QObject *>(QStringLiteral("selectedOAuthProfile"));
        QVERIFY(profileLabel);
        QCOMPARE(profileLabel->property("text").toString(), QStringLiteral("Profile: <b>Work</b>"));
        QCOMPARE(profileLabel->property("textFormat").toInt(), static_cast<int>(Qt::PlainText));
        auto *row = object->findChild<QObject *>(QStringLiteral("providerActionRow"));
        QVERIFY(row);
        QCOMPARE(row->property("actionCount").toInt(), 2);
        QVERIFY(m_openedUrls.isEmpty());
        QQuickItem *button = nullptr;
        QTRY_VERIFY(button = findVisualItem(qobject_cast<QQuickItem *>(row),
                                            QStringLiteral("provider-action-dashboard")));
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QCOMPARE(m_openedUrls,
                 QList<QUrl>{QUrl(QStringLiteral("https://chatgpt.com/codex/settings/usage"))});

        provider.insert(QStringLiteral("id"), QStringLiteral("zai"));
        provider.insert(QStringLiteral("region"), QStringLiteral("bigmodel-cn"));
        QVERIFY(object->setProperty("provider", provider));
        QCOMPARE(row->property("actionCount").toInt(), 2);
        QTRY_VERIFY(button = findVisualItem(qobject_cast<QQuickItem *>(row),
                                            QStringLiteral("provider-action-dashboard")));
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QCOMPARE(m_openedUrls.last(), QUrl(QStringLiteral("https://bigmodel.cn/")));
        provider.insert(QStringLiteral("region"), QStringLiteral("unknown"));
        QVERIFY(object->setProperty("provider", provider));
        QCOMPARE(row->property("actionCount").toInt(), 0);
        QVERIFY(!row->property("visible").toBool());
        QCOMPARE(m_openedUrls.size(), 2);
    }

  public slots:
    void recordUrl(const QUrl &url)
    {
        m_openedUrls.append(url);
    }

  private:
    QList<QUrl> m_openedUrls;
    QLibrary m_plugin{QStringLiteral(KODOMETER_APPLET_LIBRARY)};
};

QTEST_MAIN(AppletConfigurationTest)
#include "tst_applet_configuration.moc"
