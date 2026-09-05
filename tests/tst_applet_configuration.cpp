#include <QFile>
#include <QLibrary>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtTest>

#include <KConfig>
#include <KConfigGroup>
#include <KConfigLoader>

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
        QVERIFY(loader.property("disabledProviders").toStringList().isEmpty());

        QQmlComponent model(&engine, root.resolved(QUrl(QStringLiteral("config.qml"))));
        QScopedPointer<QObject> object(model.create());
        QVERIFY2(object, qPrintable(model.errorString()));
        auto *categories = qobject_cast<QAbstractItemModel *>(object.data());
        QVERIFY(categories);
        QCOMPARE(categories->rowCount(), 2);
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
            QCOMPARE(loader.items().size(), 4);
            const QVariantMap preferences{
                {QStringLiteral("autoRefresh"), false},
                {QStringLiteral("refreshIntervalMinutes"), 15},
                {QStringLiteral("disabledProviders"), QStringList{QStringLiteral("codex")}},
                {QStringLiteral("showIdleWindows"), true}};
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

  private:
    QLibrary m_plugin{QStringLiteral(KODOMETER_APPLET_LIBRARY)};
};

QTEST_MAIN(AppletConfigurationTest)
#include "tst_applet_configuration.moc"
