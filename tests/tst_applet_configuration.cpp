#include <QDesktopServices>
#include <QFile>
#include <QLibrary>
#include <QPluginLoader>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <KConfig>
#include <KConfigGroup>
#include <KConfigLoader>
#include <KConfigPropertyMap>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
    QString savedManagement;
    QString savedTeam;
    QVariantMap savedZai;
    QString removedId;
    const QString id = QStringLiteral("11111111-1111-4111-8111-111111111111");
    QVariantMap records;
    QVariantMap providers() const
    {
        return records;
    }
    Q_INVOKABLE QString addAccount(const QString &provider, const QString &name, const QString &key,
                                   const QString &management = {}, const QString &team = {},
                                   const QVariantMap &zai = {})
    {
        if (failWrites)
            return {};
        savedKey = key;
        savedManagement = management;
        savedTeam = team;
        savedZai = zai;
        records.insert(provider, QVariantList{QVariantMap{{"id", id}, {"name", name}}});
        emit changed();
        return id;
    }
    Q_INVOKABLE bool replaceAccount(const QString &, const QString &, const QString &key,
                                    const QString &management = {}, const QString &team = {},
                                    const QVariantMap &zai = {})
    {
        if (failWrites)
            return false;
        savedKey = key;
        savedManagement = management;
        savedTeam = team;
        savedZai = zai;
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

    void declaresCompiledAppletMinimum()
    {
        QPluginLoader loader(QStringLiteral(KODOMETER_APPLET_LIBRARY));
        const QJsonObject metadata = loader.metaData().value(QStringLiteral("MetaData")).toObject();
        QCOMPARE(metadata.value(QStringLiteral("X-Plasma-API-Minimum-Version")).toString(),
                 QStringLiteral("6.4"));
        QCOMPARE(metadata.value(QStringLiteral("KPlugin"))
                     .toObject()
                     .value(QStringLiteral("Icon"))
                     .toString(),
                 QStringLiteral("kodometer"));
    }

    void embedsBrandAssets()
    {
        QQmlEngine engine;
        const QUrl root(QStringLiteral("qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/"));
        QQmlComponent component(&engine, root.resolved(QUrl(QStringLiteral("ProviderIcon.qml"))));
        for (const QString &id :
             {QStringLiteral("codex"), QStringLiteral("claude"), QStringLiteral("deepseek"),
              QStringLiteral("gemini"), QStringLiteral("kimi"), QStringLiteral("openrouter"),
              QStringLiteral("xai"), QStringLiteral("zai")}) {
            QScopedPointer<QObject> icon(
                component.createWithInitialProperties({{QStringLiteral("providerId"), id},
                                                       {QStringLiteral("width"), 48},
                                                       {QStringLiteral("height"), 48}}));
            QVERIFY2(icon, qPrintable(component.errorString()));
            QTRY_COMPARE(icon->property("status").toInt(), 1); // Image.Ready, from compiled QRC
        }
        for (const QString &name : {QStringLiteral("kodometer-symbolic.svg"),
                                    QStringLiteral("kodometer-iris-on-dark.svg"),
                                    QStringLiteral("kodometer-deep-iris-on-light.svg")}) {
            const QString path =
                QStringLiteral(":/qt/qml/plasma/applet/org/kyaulabs/kodometer/assets/") + name;
            QVERIFY2(QFile::exists(path), qPrintable(path));
        }
        QQmlComponent meterComponent(&engine,
                                     root.resolved(QUrl(QStringLiteral("CompactMeter.qml"))));
        const QVariantMap quotaWindow{{QStringLiteral("kind"), QStringLiteral("session")},
                                      {QStringLiteral("remainingPercent"), 12.8}};
        const QVariantMap quotaProvider{{QStringLiteral("id"), QStringLiteral("codex")},
                                        {QStringLiteral("windows"), QVariantList{quotaWindow}}};
        QScopedPointer<QObject> meter(meterComponent.createWithInitialProperties(
            {{QStringLiteral("donutCharts"), true},
             {QStringLiteral("providers"), QVariantList{quotaProvider}}}));
        QVERIFY2(meter, qPrintable(meterComponent.errorString()));
        auto *session = findVisualItem(qobject_cast<QQuickItem *>(meter.data()),
                                       QStringLiteral("quota-ring-codex-session"));
        QVERIFY(session);
        QCOMPARE(session->property("value").toDouble(), 12.8);
    }

    void adaptsPopupHeightAcrossTabs()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*PaginatedPage.*")));
        QQmlEngine engine;
        const QUrl root(QStringLiteral("qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/"));
        const QVariantMap session{
            {"kind", "session"}, {"label", "Session"}, {"remainingPercent", 65.0}};
        const QVariantMap weekly{
            {"kind", "weekly"}, {"label", "7-day usage"}, {"remainingPercent", 30.0}};
        const QVariantMap codex{{"id", "codex"},
                                {"name", "Codex"},
                                {"windows", QVariantList{session}},
                                {"display", QVariantMap{{"accentColor", "#a28be0"}}}};
        const QVariantMap kimi{{"id", "kimi"},
                               {"name", "Kimi Code"},
                               {"windows", QVariantList{weekly, session}},
                               {"display", QVariantMap{{"accentColor", "#49a3b0"}}}};
        const QVariantMap balance{
            {"id", "deepseek"}, {"name", "DeepSeek"}, {"cost", QVariantMap{{"balance", 0.61}}}};
        QQmlComponent navigationComponent(&engine, root.resolved(QUrl("UsageNavigation.qml")));
        QScopedPointer<QObject> navigation(navigationComponent.createWithInitialProperties(
            {{"providers", QVariantList{codex, kimi, balance}}}));
        QVERIFY2(navigation, qPrintable(navigationComponent.errorString()));
        QQmlComponent switchingComponent(&engine, root.resolved(QUrl("AccountSwitching.qml")));
        const QVariantMap configuration{
            {"oauthProfiles", "{}"}, {"disabledProviders", QStringList{}},
            {"codexAccountId", ""},  {"deepseekAccountId", ""},
            {"kimiAccountId", ""},   {"openrouterAccountId", ""},
            {"xaiAccountId", ""},    {"zaiAccountId", ""}};
        QScopedPointer<QObject> switching(
            switchingComponent.createWithInitialProperties({{"configuration", configuration}}));
        QVERIFY2(switching, qPrintable(switchingComponent.errorString()));
        QQmlComponent popupComponent(&engine, root.resolved(QUrl("UsagePopup.qml")));
        QScopedPointer<QObject> popup(popupComponent.createWithInitialProperties(
            {{"navigation", QVariant::fromValue(navigation.data())},
             {"switching", QVariant::fromValue(switching.data())},
             {"availableHeight", 1400}}));
        QVERIFY2(popup, qPrintable(popupComponent.errorString()));
        auto *item = qobject_cast<QQuickItem *>(popup.data());
        QVERIFY(item);
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        item->setWidth(400);
        window.resize(400, 900);
        window.show();
        QTRY_VERIFY(item->implicitHeight() > 200);
        item->setHeight(item->implicitHeight());
        QTest::qWait(100);
        const qreal overviewHeight = item->implicitHeight();
        auto *wordmark = findVisualItem(item, "headerWordmark");
        auto *tabs = findVisualItem(item, "popupProviderTabs");
        QVERIFY(wordmark);
        QVERIFY(tabs);
        const qreal above = wordmark->mapToItem(item, QPointF{}).y();
        const qreal below = tabs->mapToItem(item, QPointF{}).y() - above - wordmark->height();
        QVERIFY(qAbs(above - below) <= 2);

        connect(item, &QQuickItem::implicitHeightChanged, &window, [&] {
            item->setHeight(item->implicitHeight());
            window.resize(400, qCeil(item->height()));
        });
        const auto capture = [&](const QString &name) {
            item->setHeight(item->implicitHeight());
            window.resize(400, qCeil(item->height()));
            QTest::qWait(100);
            auto *footer = findVisualItem(item, "popupFooter");
            QVERIFY(footer);
            auto *settings = findVisualItem(item, "configureAccounts");
            auto *refresh = findVisualItem(item, "refreshUsage");
            QVERIFY(settings);
            QVERIFY(refresh);
            QCOMPARE(QQmlProperty::read(settings, "icon.name").toString(),
                     QStringLiteral("configure-symbolic"));
            QCOMPARE(QQmlProperty::read(settings, "icon.width"),
                     QQmlProperty::read(refresh, "icon.width"));
            QCOMPARE(QQmlProperty::read(settings, "icon.height"),
                     QQmlProperty::read(refresh, "icon.height"));
            const qreal footerBottom = footer->mapToItem(item, QPointF(0, footer->height())).y();
            QVERIFY2(footerBottom <= item->height(),
                     qPrintable(QStringLiteral("%1: footer %2, popup %3, preferred %4")
                                    .arg(name)
                                    .arg(footerBottom)
                                    .arg(item->height())
                                    .arg(item->implicitHeight())));
            auto *viewport = findVisualItem(item, "sectionPageViewport");
            QVERIFY(viewport);
            const qreal contentBottom =
                viewport->mapToItem(item, QPointF(0, viewport->height())).y();
            const qreal footerTop = footer->mapToItem(item, QPointF()).y();
            QVERIFY2(contentBottom <= footerTop,
                     qPrintable(QStringLiteral("%1: content %2, footer %3")
                                    .arg(name)
                                    .arg(contentBottom)
                                    .arg(footerTop)));
            const QString directory = qEnvironmentVariable("KODOMETER_UI_CAPTURE_DIR");
            if (!directory.isEmpty()) {
                QVERIFY(QDir().mkpath(directory));
                QVERIFY(window.grabWindow().save(QDir(directory).filePath(name + ".png")));
            }
        };
        capture("overview");
        QVERIFY(QMetaObject::invokeMethod(navigation.data(), "selectProvider",
                                          Q_ARG(QVariant, QStringLiteral("codex"))));
        QTRY_VERIFY(item->implicitHeight() != overviewHeight);
        const qreal codexHeight = item->implicitHeight();
        auto *chart = findVisualItem(item, QStringLiteral("quotaHistoryChart"));
        QVERIFY(chart);
        QVariantList observed;
        const qint64 end = QDateTime::currentSecsSinceEpoch();
        for (int i = 0; i < 288; ++i)
            observed.append(
                QVariant(QVariantList{end - (287 - i) * 300, 65.0 + ((287 - i) % 48) * 0.4, 0}));
        QVERIFY(chart->setProperty(
            "series", QVariantList{QVariantMap{{"kind", "session"}, {"points", observed}}}));
        capture("codex");
        QVERIFY(popup->setProperty("availableHeight", 500));
        auto *pagedLoader = findVisualItem(item, "popupPageLoader");
        QVERIFY(pagedLoader);
        auto *paged = pagedLoader->property("item").value<QObject *>();
        QVERIFY(paged);
        QTRY_VERIFY(paged->property("pageCount").toInt() > 1);
        for (int index = 0; index < paged->property("pageCount").toInt(); ++index) {
            QVERIFY(paged->setProperty("currentPage", index));
            QTRY_VERIFY(item->implicitHeight() <= 425);
            capture(QStringLiteral("codex-page-%1").arg(index + 1));
        }
        QVERIFY(popup->setProperty("availableHeight", 1400));
        QTRY_COMPARE(paged->property("pageCount").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(navigation.data(), "selectProvider",
                                          Q_ARG(QVariant, QStringLiteral("deepseek"))));
        QTRY_VERIFY(item->implicitHeight() < codexHeight);
        capture("deepseek");
        QVERIFY(popup->setProperty("availableHeight", 300));
        QTRY_VERIFY(item->implicitHeight() <= 255);
        auto *loader = findVisualItem(item, "popupPageLoader");
        QVERIFY(loader);
        auto *page = loader->property("item").value<QObject *>();
        QVERIFY(page);
        QVERIFY(page->property("contentHeight").toDouble() > 0);
    }

    void confirmsHistoryClearWithoutProviderData()
    {
        QQmlEngine engine;
        const QUrl root(QStringLiteral("qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/"));
        QQmlComponent navigationComponent(&engine, root.resolved(QUrl("UsageNavigation.qml")));
        QScopedPointer<QObject> navigation(navigationComponent.create());
        QVERIFY2(navigation, qPrintable(navigationComponent.errorString()));
        QQmlComponent historyComponent(&engine);
        historyComponent.setData(R"(import QtQml
            QtObject {
                property int revision: 0
                property string error: ""
                property bool cleared: false
                function view(provider) { return [] }
                function clear() { cleared = true }
            })",
                                 root.resolved(QUrl("test-history.qml")));
        QScopedPointer<QObject> history(historyComponent.create());
        QVERIFY2(history, qPrintable(historyComponent.errorString()));
        QQmlComponent component(&engine, root.resolved(QUrl("UsagePopup.qml")));
        QScopedPointer<QObject> popup(component.createWithInitialProperties(
            {{"navigation", QVariant::fromValue(navigation.data())},
             {"quotaHistory", QVariant::fromValue(history.data())}}));
        QVERIFY2(popup, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(popup.data());
        QVERIFY(item);
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(400, 500));
        window.resize(400, 500);
        window.show();
        auto *button = findVisualItem(item, "clearQuotaHistory");
        auto *dialog = popup->findChild<QObject *>("clearHistoryConfirmation");
        QVERIFY(button);
        QVERIFY(dialog);
        QVERIFY(button->isVisible());
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(!history->property("cleared").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "reject"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(!history->property("cleared").toBool());
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "accept"));
        QVERIFY(history->property("cleared").toBool());
    }

    void excludesMonetaryCapsFromSubscriptionCharts()
    {
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral(
                         "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ProviderDetails.qml")));
        const QVariantMap cap{{"kind", "spend-limit"}, {"remainingPercent", 65.0}};
        QVariantMap provider{{"id", "claude"}, {"windows", QVariantList{cap}}};
        QScopedPointer<QObject> object(
            component.createWithInitialProperties({{"width", 400}, {"provider", provider}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *chart =
            findVisualItem(qobject_cast<QQuickItem *>(object.data()), "quotaHistoryChart");
        QVERIFY(chart);
        QVERIFY(!chart->isVisible());
        provider["windows"] =
            QVariantList{cap, QVariantMap{{"kind", "session"}, {"remainingPercent", 50.0}}};
        QVERIFY(object->setProperty("provider", provider));
        QVERIFY(chart->isVisible());
        QCOMPARE(chart->property("windows").value<QJSValue>().property("length").toInt(), 1);
    }

    void appliesProviderAccentsToTabsBarsAndCharts()
    {
        QQmlEngine engine;
        const QUrl root(QStringLiteral("qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/"));
        const QVariantMap provider{
            {"id", "codex"},
            {"name", "Codex"},
            {"display", QVariantMap{{"accentColor", "#123456"}}},
            {"windows",
             QVariantList{QVariantMap{{"kind", "session"}, {"remainingPercent", 45.0}}}}};
        QQmlComponent tabsComponent(&engine, root.resolved(QUrl("ProviderTabs.qml")));
        QScopedPointer<QObject> tabs(tabsComponent.createWithInitialProperties(
            {{"width", 400}, {"providers", QVariantList{provider}}, {"overviewVisible", true}}));
        QVERIFY2(tabs, qPrintable(tabsComponent.errorString()));
        auto *tabsItem = qobject_cast<QQuickItem *>(tabs.data());
        QQuickWindow window;
        tabsItem->setParentItem(window.contentItem());
        window.show();
        QQuickItem *accent = nullptr;
        QTRY_VERIFY((accent = findVisualItem(tabsItem, "provider-tab-accent-1")));
        QCOMPARE(accent->property("color").value<QColor>(), QColor("#123456"));
        auto *overviewAccent = findVisualItem(tabsItem, "provider-tab-accent-0");
        QVERIFY(overviewAccent);
        QVERIFY(overviewAccent->isVisible());
        QCOMPARE(overviewAccent->property("color"), tabs->property("overviewAccentColor"));
        auto *tabBrand = findVisualItem(tabsItem, "tabBrandPalette");
        auto *overviewTab = findVisualItem(tabsItem, "provider-tab-0");
        QVERIFY(tabBrand);
        QVERIFY(overviewTab);
        QVERIFY(tabBrand->setProperty("surfaceColor", QColor("#252333")));
        QTRY_COMPARE(
            findVisualItem(tabsItem, "provider-tab-accent-0")->property("color").value<QColor>(),
            QColor("#F7F5FB"));
        overviewTab = findVisualItem(tabsItem, "provider-tab-0");
        QCOMPARE(QQmlProperty::read(overviewTab, "palette.highlight").value<QColor>(),
                 QColor("#F7F5FB"));
        QVERIFY(tabBrand->setProperty("surfaceColor", QColor("#ffffff")));
        QTRY_COMPARE(
            findVisualItem(tabsItem, "provider-tab-accent-0")->property("color").value<QColor>(),
            QColor("#252333"));

        QQmlComponent meterComponent(&engine, root.resolved(QUrl("CompactMeter.qml")));
        QScopedPointer<QObject> meter(meterComponent.createWithInitialProperties(
            {{"providers", QVariantList{provider}}, {"donutCharts", true}}));
        QVERIFY2(meter, qPrintable(meterComponent.errorString()));
        QCOMPARE(meter->metaObject()->indexOfProperty("sessionColor"), -1);
        QCOMPARE(meter->metaObject()->indexOfProperty("weeklyColor"), -1);
        auto *ring =
            findVisualItem(qobject_cast<QQuickItem *>(meter.data()), "quota-ring-codex-session");
        QVERIFY(ring);
        QCOMPARE(ring->property("accentColor").value<QColor>(), QColor("#123456"));
        QVariant barColor;
        QVERIFY(QMetaObject::invokeMethod(meter.data(), "barColor",
                                          Q_RETURN_ARG(QVariant, barColor),
                                          Q_ARG(QVariant, "session")));
        QCOMPARE(barColor.toString(), QStringLiteral("#123456"));

        QQmlComponent detailsComponent(&engine, root.resolved(QUrl("ProviderDetails.qml")));
        QScopedPointer<QObject> details(
            detailsComponent.createWithInitialProperties({{"provider", provider}, {"width", 400}}));
        QVERIFY2(details, qPrintable(detailsComponent.errorString()));
        for (const auto &name : {"detailQuotaBar", "quotaHistoryChart", "costHistory"}) {
            auto *part = findVisualItem(qobject_cast<QQuickItem *>(details.data()), name);
            QVERIFY(part);
            QCOMPARE(part->property("accentColor").value<QColor>(), QColor("#123456"));
        }
    }

    void keepsProviderControlsBelowContentAndSelectionInSettings()
    {
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral(
                         "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ProviderDetails.qml")));
        const QVariantMap provider{
            {"id", "kimi"},
            {"name", "Kimi Code"},
            {"windows",
             QVariantList{QVariantMap{{"kind", "session"}, {"remainingPercent", 100.0}}}}};
        QScopedPointer<QObject> object(
            component.createWithInitialProperties({{"width", 400}, {"provider", provider}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        window.show();
        QVERIFY(!findVisualItem(item, "detailAccountSelector"));
        QVERIFY(!findVisualItem(item, "clearQuotaHistory"));
        auto *chart = findVisualItem(item, "quotaHistoryChart");
        auto *actions = findVisualItem(item, "providerActionRow");
        QVERIFY(chart);
        QVERIFY(actions);
        QTRY_VERIFY(actions->y() >= chart->y() + chart->height());
    }

    void containsSummaryContentWithPadding()
    {
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral(
                         "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ProviderSummary.qml")));
        const QVariantMap provider{
            {"id", "codex"},
            {"name", "Codex"},
            {"bankedResets", 2},
            {"windows",
             QVariantList{QVariantMap{{"kind", "session"}, {"remainingPercent", 45.0}}}}};
        QScopedPointer<QObject> object(
            component.createWithInitialProperties({{"width", 380}, {"provider", provider}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        item->setHeight(item->implicitHeight());
        window.show();
        auto *bar = findVisualItem(item, "summaryQuotaBar");
        QVERIFY(bar);
        QTRY_VERIFY(bar->height() > 0);
        QTest::qWait(100);
        const auto bottom = bar->mapToItem(item, QPointF(0, bar->height())).y();
        QVERIFY2(item->height() - bottom >= 6,
                 qPrintable(QString::number(item->height() - bottom)));
        QVERIFY(bar->mapToItem(item, QPointF(bar->width(), 0)).x() <= item->width() - 6);
    }

    void fillsBarsWithRemainingOrUsedQuota()
    {
        QQmlEngine engine;
        const QUrl root(QStringLiteral("qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/"));
        const QVariantMap window{{"kind", "session"}, {"remainingPercent", 65.0}};
        const QVariantMap provider{{"id", "codex"}, {"windows", QVariantList{window}}};
        for (const auto &name :
             {QStringLiteral("WindowRow.qml"), QStringLiteral("ProviderSummary.qml")}) {
            const bool detail = name == QLatin1String("WindowRow.qml");
            QQmlComponent component(&engine, root.resolved(QUrl(name)));
            QVariantMap properties{{"width", 300}};
            properties.insert(detail ? QStringLiteral("windowData") : QStringLiteral("provider"),
                              detail ? window : provider);
            QScopedPointer<QObject> object(component.createWithInitialProperties(properties));
            QVERIFY2(object, qPrintable(component.errorString()));
            auto *bar = findVisualItem(qobject_cast<QQuickItem *>(object.data()),
                                       detail ? QStringLiteral("detailQuotaBar")
                                              : QStringLiteral("summaryQuotaBar"));
            QVERIFY(bar);
            QCOMPARE(bar->property("percent").toDouble(), 65.0);
            QVERIFY(object->setProperty("fillRemaining", false));
            QCOMPARE(bar->property("percent").toDouble(), 35.0);
            QCOMPARE(bar->property("implicitHeight").toDouble(), 10.0);
        }
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
        QVERIFY(loader.findItemByName(QStringLiteral("panelDonutCharts")));
        QVERIFY(loader.findItemByName(QStringLiteral("panelSystemAccent")));
        QCOMPARE(loader.property("panelDonutCharts").toBool(), false);
        QCOMPARE(loader.property("panelSystemAccent").toBool(), false);
        QVERIFY(loader.property("panelSessionColor").toString().isEmpty());
        QVERIFY(loader.property("panelWeeklyColor").toString().isEmpty());
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
        QVERIFY(loader.property("openrouterAccountId").toString().isEmpty());
        QVERIFY(loader.property("xaiAccountId").toString().isEmpty());
        QVERIFY(loader.property("zaiAccountId").toString().isEmpty());
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

    void loadsCompiledGeneralAndColorControls()
    {
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral(
                         "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ConfigGeneral.qml")));
        QScopedPointer<QObject> page(component.create());
        QVERIFY2(page, qPrintable(component.errorString()));
        QVERIFY(page->property("flickable").value<QObject *>());
        QVERIFY(page->setProperty("cfg_panelDonutCharts", true));
        QVERIFY(page->setProperty("cfg_providerColors", QStringLiteral("{\"codex\":\"#112233\"}")));
        auto *codex = findVisualItem(qobject_cast<QQuickItem *>(page.data()),
                                     QStringLiteral("provider-color-codex"));
        QVERIFY(codex);
        QCOMPARE(codex->property("colorValue").toString(), QStringLiteral("#112233"));
        QVERIFY(!findVisualItem(qobject_cast<QQuickItem *>(page.data()),
                                QStringLiteral("panelSessionColor")));
        QVERIFY(!findVisualItem(qobject_cast<QQuickItem *>(page.data()),
                                QStringLiteral("panelWeeklyColor")));
    }

    void persistsOnlyNonsecretPreferences()
    {
        QTemporaryDir temporary;
        const QString path = temporary.filePath(QStringLiteral("settings"));
        QFile schema(QStringLiteral(":/qt/qml/plasma/applet/org/kyaulabs/kodometer/main.xml"));
        {
            KConfig config(path, KConfig::SimpleConfig);
            KConfigLoader loader(KConfigGroup(&config, "Widget"), &schema);
            QCOMPARE(loader.items().size(), 21);
            const QVariantMap preferences{
                {QStringLiteral("autoRefresh"), false},
                {QStringLiteral("refreshIntervalMinutes"), 15},
                {QStringLiteral("disabledProviders"), QStringList{QStringLiteral("codex")}},
                {QStringLiteral("showIdleWindows"), true},
                {QStringLiteral("showCodexSpark"), true},
                {QStringLiteral("quotaBarsRemaining"), false},
                {QStringLiteral("hiddenQuotaWindows"), QStringList{QStringLiteral("codex/weekly")}},
                {QStringLiteral("quotaWindowCatalog"), QStringLiteral("[]")},
                {QStringLiteral("panelDonutCharts"), true},
                {QStringLiteral("panelSystemAccent"), true},
                {QStringLiteral("panelSessionColor"), QStringLiteral("#112233")},
                {QStringLiteral("panelWeeklyColor"), QStringLiteral("#aabbcc")},
                {QStringLiteral("providerColors"), QStringLiteral("{\"codex\":\"#123456\"}")},
                {QStringLiteral("quotaNotifications"), true},
                {QStringLiteral("quotaNotificationThreshold"), 20},
                {QStringLiteral("deepseekAccountId"),
                 QStringLiteral("11111111-1111-4111-8111-111111111111")},
                {QStringLiteral("kimiAccountId"),
                 QStringLiteral("22222222-2222-4222-8222-222222222222")},
                {QStringLiteral("openrouterAccountId"),
                 QStringLiteral("33333333-3333-4333-8333-333333333333")},
                {QStringLiteral("xaiAccountId"),
                 QStringLiteral("44444444-4444-4444-8444-444444444444")},
                {QStringLiteral("zaiAccountId"),
                 QStringLiteral("55555555-5555-4555-8555-555555555555")},
                {QStringLiteral("oauthProfiles"),
                 QStringLiteral(
                     R"({"version":1,"codex":[{"id":"11111111-1111-4111-8111-111111111111","name":"Work","directory":"/profiles/work"}],"selectedCodex":"11111111-1111-4111-8111-111111111111","gemini":[{"id":"22222222-2222-4222-8222-222222222222","name":"Gemini Work","directory":"/profiles/gemini"}],"selectedGemini":"22222222-2222-4222-8222-222222222222"})")}};
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
        QCOMPARE(reloaded.property("showCodexSpark").toBool(), true);
        QCOMPARE(reloaded.property("quotaBarsRemaining").toBool(), false);
        QCOMPARE(reloaded.property("hiddenQuotaWindows").toStringList(),
                 QStringList{QStringLiteral("codex/weekly")});
        QCOMPARE(reloaded.property("panelDonutCharts").toBool(), true);
        QCOMPARE(reloaded.property("panelSystemAccent").toBool(), true);
        QCOMPARE(reloaded.property("panelSessionColor").toString(), QStringLiteral("#112233"));
        QCOMPARE(reloaded.property("panelWeeklyColor").toString(), QStringLiteral("#aabbcc"));
        QCOMPARE(reloaded.property("providerColors").toString(),
                 QStringLiteral("{\"codex\":\"#123456\"}"));
        QCOMPARE(reloaded.property("quotaNotifications").toBool(), true);
        QCOMPARE(reloaded.property("quotaNotificationThreshold").toInt(), 20);
        QCOMPARE(reloaded.property("deepseekAccountId").toString(),
                 QStringLiteral("11111111-1111-4111-8111-111111111111"));
        QCOMPARE(reloaded.property("kimiAccountId").toString(),
                 QStringLiteral("22222222-2222-4222-8222-222222222222"));
        QCOMPARE(reloaded.property("openrouterAccountId").toString(),
                 QStringLiteral("33333333-3333-4333-8333-333333333333"));
        QCOMPARE(reloaded.property("xaiAccountId").toString(),
                 QStringLiteral("44444444-4444-4444-8444-444444444444"));
        QCOMPARE(reloaded.property("zaiAccountId").toString(),
                 QStringLiteral("55555555-5555-4555-8555-555555555555"));
        QVERIFY(reloaded.property("oauthProfiles")
                    .toString()
                    .contains(QStringLiteral("/profiles/work")));
        QVERIFY(reloaded.property("oauthProfiles")
                    .toString()
                    .contains(QStringLiteral("/profiles/gemini")));
    }

    void switchesThroughPersistentConfiguration_data()
    {
        QTest::addColumn<QString>("provider");
        for (const QString &id :
             {QStringLiteral("codex"), QStringLiteral("claude"), QStringLiteral("gemini"),
              QStringLiteral("deepseek"), QStringLiteral("kimi"), QStringLiteral("openrouter"),
              QStringLiteral("xai"), QStringLiteral("zai")})
            QTest::newRow(qPrintable(id)) << id;
    }

    void switchesThroughPersistentConfiguration()
    {
        QFETCH(QString, provider);
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        const bool oauth = provider == "codex" || provider == "claude" || provider == "gemini";
        const QString id = QStringLiteral("11111111-1111-4111-8111-111111111111");
        const QString selectionKey = "selected" + provider.left(1).toUpper() + provider.mid(1);
        const QString original = QString::fromUtf8(
            QJsonDocument(
                QJsonObject{{provider, QJsonArray{QJsonObject{{"id", id},
                                                              {"name", "<b>Work</b>"},
                                                              {"directory", "/profiles/work"}}}},
                            {selectionKey, id}})
                .toJson(QJsonDocument::Compact));
        QTemporaryDir temporary;
        const QString path = temporary.filePath("widgetrc");
        KConfig config(path, KConfig::SimpleConfig);
        KConfigGroup widgetGroup(&config, "Widget");
        KConfigGroup group(&widgetGroup, "General");
        group.writeEntry("oauthProfiles", oauth ? original : QStringLiteral("{}"));
        const QString field = oauth ? QStringLiteral("oauthProfiles") : provider + "AccountId";
        if (!oauth)
            group.writeEntry(qPrintable(field), id);
        group.writeEntry("refreshIntervalMinutes", 17);
        config.sync();
        QFile schema(QStringLiteral(":/qt/qml/plasma/applet/org/kyaulabs/kodometer/main.xml"));
        KConfigLoader loader(widgetGroup, &schema);
        KConfigPropertyMap preferences(&loader);
        QQmlEngine engine;
        QQmlComponent backendComponent(&engine);
        backendComponent.setData(R"(import plasma.applet.org.kyaulabs.kodometer as Private
            Private.UsageController {
                required property var preferences
                autoRefresh: false
                profiles.configuration: preferences.oauthProfiles
                deepseekAccountId: preferences.deepseekAccountId
                kimiAccountId: preferences.kimiAccountId
                openrouterAccountId: preferences.openrouterAccountId
                xaiAccountId: preferences.xaiAccountId
                zaiAccountId: preferences.zaiAccountId
            })",
                                 QUrl());
        QScopedPointer<QObject> backend(backendComponent.createWithInitialProperties(
            {{"preferences", QVariant::fromValue<QObject *>(&preferences)}}));
        QVERIFY2(backend, qPrintable(backendComponent.errorString()));
        QQmlComponent component(
            &engine, QUrl(QStringLiteral(
                         "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/AccountSwitching.qml")));
        QScopedPointer<QObject> switching(component.createWithInitialProperties(
            {{"configuration", QVariant::fromValue<QObject *>(&preferences)}}));
        QVERIFY2(switching, qPrintable(component.errorString()));
        QSignalSpy applied(switching.data(), SIGNAL(selectionApplied(QString)));
        QQmlComponent selectorComponent(
            &engine, QUrl(QStringLiteral(
                         "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/AccountSelector.qml")));
        QScopedPointer<QObject> selectorObject(selectorComponent.createWithInitialProperties(
            {{"switching", QVariant::fromValue(switching.data())},
             {"providerId", provider},
             {"width", 260},
             {"height", 60}}));
        QVERIFY2(selectorObject, qPrintable(selectorComponent.errorString()));
        auto *selector = selectorObject->findChild<QQuickItem *>("runtime-account-selector");
        QVERIFY(selector);
        QCOMPARE(selector->property("currentIndex").toInt(), 1);
        auto *label = selector->property("contentItem").value<QObject *>();
        QVERIFY(label);
        // Keep the style's plain-text input; a replacement label double-paints in KDE Desktop.
        QVERIFY(label->inherits("QQuickTextInput"));
        if (oauth)
            QCOMPARE(label->property("text").toString(), QStringLiteral("<b>Work</b>"));
        QQuickWindow window;
        window.resize(260, 60);
        qobject_cast<QQuickItem *>(selectorObject.data())->setParentItem(window.contentItem());
        window.show();
        selector->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Home);
        QTRY_COMPARE(applied.count(), 1);
        KConfig saved(path, KConfig::SimpleConfig);
        KConfigGroup savedWidget(&saved, "Widget");
        KConfigGroup persisted(&savedWidget, "General");
        QCOMPARE(persisted.readEntry("refreshIntervalMinutes", 0), 17);
        if (oauth) {
            const auto document =
                QJsonDocument::fromJson(persisted.readEntry("oauthProfiles", QString{}).toUtf8())
                    .object();
            QCOMPARE(document.value(selectionKey).toString(), QStringLiteral("default"));
            QCOMPARE(document.value(provider).toArray().size(), 1);
        }
        else {
            QVERIFY(persisted.readEntry(qPrintable(field), QString{}).isEmpty());
        }
        // An external settings update must still drive the selector after a runtime switch.
        QVERIFY(preferences.setProperty(qPrintable(field), oauth ? original : id));
        QTRY_COMPARE(selector->property("currentIndex").toInt(), 1);
        QVariant result;
        QVERIFY(QMetaObject::invokeMethod(switching.data(), "select",
                                          Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, provider),
                                          Q_ARG(QVariant, QStringLiteral("missing"))));
        QVERIFY(!result.toBool());
        QVERIFY(!switching->property("error").toString().isEmpty());
        QCOMPARE(selector->property("currentIndex").toInt(), 1);
        selector->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Home);
        QTRY_COMPARE(applied.count(), 2);
        QVERIFY(switching->property("error").toString().isEmpty());
        if (oauth) {
            auto *profiles = backend->property("profiles").value<QObject *>();
            QVERIFY(profiles);
            QCOMPARE(profiles->property("providers")
                         .toMap()
                         .value(provider)
                         .toMap()
                         .value("selectedId")
                         .toString(),
                     QStringLiteral("default"));
        }
        else {
            QVERIFY(backend->property(qPrintable(field)).toString().isEmpty());
        }
        QVERIFY(!backend->property("busy").toBool());
    }

    void opensSwitcherWithoutUsageOrImplicitWrites()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QQmlEngine engine;
        QQmlComponent preferencesComponent(&engine);
        preferencesComponent.setData(R"(import QtQml
            QtObject {
                property string oauthProfiles: "{}"
                property string deepseekAccountId: "missing"
                property string kimiAccountId: ""
                property string openrouterAccountId: ""
                property string xaiAccountId: ""
                property string zaiAccountId: ""
                property var disabledProviders: []
                property int writes: 0
                function writeConfig() { writes++ }
            })",
                                     QUrl());
        QScopedPointer<QObject> preferences(preferencesComponent.create());
        QVERIFY(preferences);
        const QUrl base(QStringLiteral("qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/"));
        QQmlComponent bridgeComponent(&engine, base.resolved(QUrl("AccountSwitching.qml")));
        QScopedPointer<QObject> bridge(bridgeComponent.createWithInitialProperties(
            {{"configuration", QVariant::fromValue(preferences.data())}}));
        QVERIFY2(bridge, qPrintable(bridgeComponent.errorString()));
        QQuickWindow window;
        window.resize(420, 600);
        window.show();
        QQmlComponent dialogComponent(&engine, base.resolved(QUrl("AccountSwitchDialog.qml")));
        QScopedPointer<QObject> dialog(dialogComponent.createWithInitialProperties(
            {{"switching", QVariant::fromValue(bridge.data())},
             {"parent", QVariant::fromValue(window.contentItem())},
             {"initialProviderId", "deepseek"}}));
        QVERIFY2(dialog, qPrintable(dialogComponent.errorString()));
        QSignalSpy wallet(dialog.data(), SIGNAL(walletRequested()));
        QSignalSpy configure(dialog.data(), SIGNAL(configureRequested()));
        QVERIFY(QMetaObject::invokeMethod(dialog.data(), "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        QCOMPARE(preferences->property("writes").toInt(), 0);
        QCOMPARE(dialog->property("targetProviderId").toString(), QStringLiteral("deepseek"));
        auto *retry = dialog->findChild<QObject *>("switch-wallet-retry");
        auto *provider = dialog->findChild<QObject *>("switch-provider-selector");
        auto *account = dialog->findChild<QObject *>("runtime-account-selector");
        QVERIFY(retry && provider && account);
        QVERIFY(QMetaObject::invokeMethod(retry, "clicked"));
        QCOMPARE(wallet.count(), 1);
        QVERIFY(provider->setProperty("currentIndex", 0));
        QVERIFY(QMetaObject::invokeMethod(provider, "activated", Q_ARG(int, 0)));
        QCOMPARE(preferences->property("writes").toInt(), 0);
        QCOMPARE(dialog->property("targetProviderId").toString(), QStringLiteral("codex"));
        // Selecting the already selected Default closes the dialog without a write.
        QVERIFY(QMetaObject::invokeMethod(account, "activated", Q_ARG(int, 0)));
        QTRY_VERIFY(!dialog->property("opened").toBool());
        QCOMPARE(preferences->property("writes").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(dialog.data(), "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(account->setProperty("currentIndex", 0));
        QVERIFY(QMetaObject::invokeMethod(account, "activated", Q_ARG(int, 0)));
        QCOMPARE(preferences->property("deepseekAccountId").toString(), QString{});
        QCOMPARE(preferences->property("writes").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(dialog.data(), "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *settings = dialog->findChild<QObject *>("switch-configure");
        QVERIFY(settings);
        QVERIFY(QMetaObject::invokeMethod(settings, "clicked"));
        QCOMPARE(configure.count(), 1);
    }

    void retainsSwitchTargetWithoutRetainingUsage()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QQmlEngine engine;
        QQmlComponent component(
            &engine, QUrl(QStringLiteral(
                         "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/UsageNavigation.qml")));
        QScopedPointer<QObject> navigation(component.create());
        QVERIFY2(navigation, qPrintable(component.errorString()));
        const QVariantList catalog{QVariantMap{{"id", "codex"}, {"name", "Codex"}},
                                   QVariantMap{{"id", "kimi"}, {"name", "Kimi"}}};
        navigation->setProperty("catalog", catalog);
        navigation->setProperty(
            "providers",
            QVariantList{QVariantMap{{"id", "kimi"}, {"cost", QVariantMap{{"balance", 40}}}}});
        QVERIFY(QMetaObject::invokeMethod(navigation.data(), "focusProvider",
                                          Q_ARG(QVariant, QStringLiteral("codex"))));
        QTRY_COMPARE(navigation->property("selectedProviderId").toString(),
                     QStringLiteral("codex"));
        auto selected = navigation->property("selectedProvider").toMap();
        QVERIFY(selected.value("pendingSelection").toBool());
        QVERIFY(!selected.contains("cost"));
        navigation->setProperty("providers",
                                QVariantList{QVariantMap{{"id", "codex"}, {"fresh", true}}});
        QTRY_VERIFY(navigation->property("selectedProvider").toMap().value("fresh").toBool());
        navigation->setProperty("providers", QVariantList{});
        QTRY_VERIFY(
            navigation->property("selectedProvider").toMap().value("pendingSelection").toBool());
        // Explicit navigation wins over queued restoration.
        QVERIFY(QMetaObject::invokeMethod(navigation.data(), "selectOverview"));
        QCoreApplication::processEvents();
        QVERIFY(navigation->property("requestedProviderId").toString().isEmpty());
        QVERIFY(navigation->property("displayedProviders").toList().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(navigation.data(), "focusProvider",
                                          Q_ARG(QVariant, QStringLiteral("codex"))));
        navigation->setProperty("catalog", QVariantList{});
        QCoreApplication::processEvents();
        QVERIFY(navigation->property("requestedProviderId").toString().isEmpty());
        QVERIFY(navigation->property("selectedProvider").toMap().isEmpty());
    }

    void stagesProfileEditsUntilApplied_data()
    {
        QTest::addColumn<QString>("provider");
        QTest::newRow("codex") << QStringLiteral("codex");
        QTest::newRow("claude") << QStringLiteral("claude");
        QTest::newRow("gemini") << QStringLiteral("gemini");
    }

    void stagesProfileEditsUntilApplied()
    {
        QFETCH(QString, provider);
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
        auto *name = page->findChild<QObject *>(provider + QStringLiteral("-profile-name"));
        auto *directory =
            page->findChild<QObject *>(provider + QStringLiteral("-profile-directory"));
        auto *add = page->findChild<QObject *>(provider + QStringLiteral("-profile-add"));
        QVERIFY(name);
        QVERIFY(directory);
        QVERIFY(add);
        QVERIFY(name->setProperty("text", QStringLiteral("Work")));
        QVERIFY(directory->setProperty("text", QStringLiteral("/profiles/work")));
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        const QString staged = page->property("cfg_oauthProfiles").toString();
        QVERIFY(staged.contains(QStringLiteral("/profiles/work")));
        QCOMPARE(liveProfiles->property("configuration").toString(), QStringLiteral("{}"));
        auto *selector = page->findChild<QObject *>(provider + QStringLiteral("-profile-selector"));
        auto *remove = page->findChild<QObject *>(provider + QStringLiteral("-profile-remove"));
        QVERIFY(selector);
        QVERIFY(remove);
        auto *profileField = selector->property("contentItem").value<QObject *>();
        QVERIFY(profileField && profileField->inherits("QQuickTextInput"));
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
        for (const QString &provider :
             {QStringLiteral("deepseek"), QStringLiteral("kimi"), QStringLiteral("openrouter"),
              QStringLiteral("xai"), QStringLiteral("zai")}) {
            auto *accountSelector =
                page->findChild<QObject *>(provider + QStringLiteral("-wallet-selector"));
            QVERIFY(accountSelector);
            auto *field = accountSelector->property("contentItem").value<QObject *>();
            QVERIFY(field && field->inherits("QQuickTextInput"));
        }
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

    void stagesOpenRouterSelectionAndClearsBothPasswordFields()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QQmlEngine engine;
        AccountUiStore accounts;
        QQmlComponent component(
            &engine,
            QUrl(QStringLiteral(
                "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ConfigWalletAccounts.qml")));
        QScopedPointer<QObject> page(component.createWithInitialProperties(
            {{"accountStore", QVariant::fromValue<QObject *>(&accounts)}}));
        QVERIFY2(page, qPrintable(component.errorString()));
        QVERIFY(page->property("cfg_openrouterAccountId").toString().isEmpty());
        auto *name = page->findChild<QObject *>(QStringLiteral("openrouter-wallet-name"));
        auto *key = page->findChild<QObject *>(QStringLiteral("openrouter-wallet-key"));
        auto *management =
            page->findChild<QObject *>(QStringLiteral("openrouter-wallet-management-key"));
        auto *add = page->findChild<QObject *>(QStringLiteral("openrouter-wallet-add"));
        auto *replace = page->findChild<QObject *>(QStringLiteral("openrouter-wallet-replace"));
        QVERIFY(name && key && management && add && replace);
        QCOMPARE(management->property("echoMode").toInt(), 2);
        QVERIFY(name->setProperty("text", QStringLiteral("Work")));
        QVERIFY(key->setProperty("text", QStringLiteral("ordinary")));
        QVERIFY(management->setProperty("text", QStringLiteral("management")));
        accounts.failWrites = true;
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        QCOMPARE(management->property("text").toString(), QStringLiteral("management"));
        accounts.failWrites = false;
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        QCOMPARE(accounts.savedKey, QStringLiteral("ordinary"));
        QCOMPARE(accounts.savedManagement, QStringLiteral("management"));
        QCOMPARE(page->property("cfg_openrouterAccountId").toString(), accounts.id);
        QVERIFY(key->property("text").toString().isEmpty());
        QVERIFY(management->property("text").toString().isEmpty());
        QVERIFY(key->setProperty("text", QStringLiteral("replacement")));
        QVERIFY(QMetaObject::invokeMethod(replace, "clicked"));
        QVERIFY(accounts.savedManagement.isEmpty());
        QVERIFY(management->setProperty("text", QStringLiteral("draft")));
        QVERIFY(key->setProperty("text", QStringLiteral("draft")));
        QVERIFY(page->setProperty("cfg_openrouterAccountId", QString{}));
        QVERIFY(management->property("text").toString().isEmpty());
        QVERIFY(key->property("text").toString().isEmpty());
        QVERIFY(management->setProperty("text", QStringLiteral("draft")));
        QVERIFY(accounts.setProperty("ready", false));
        QVERIFY(management->property("text").toString().isEmpty());
        page.reset();
        QCOMPARE(accounts.savedKey,
                 QStringLiteral("replacement")); // Cancel does not undo wallet writes.
    }

    void stagesXaiSelectionAndRequiresBothInputs()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QQmlEngine engine;
        AccountUiStore accounts;
        QQmlComponent component(
            &engine,
            QUrl(QStringLiteral(
                "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ConfigWalletAccounts.qml")));
        QScopedPointer<QObject> page(component.createWithInitialProperties(
            {{"accountStore", QVariant::fromValue<QObject *>(&accounts)}}));
        QVERIFY2(page, qPrintable(component.errorString()));
        QVERIFY(page->property("cfg_xaiAccountId").toString().isEmpty());
        auto *name = page->findChild<QObject *>(QStringLiteral("xai-wallet-name"));
        auto *key = page->findChild<QObject *>(QStringLiteral("xai-wallet-key"));
        auto *team = page->findChild<QObject *>(QStringLiteral("xai-wallet-team"));
        auto *add = page->findChild<QObject *>(QStringLiteral("xai-wallet-add"));
        auto *replace = page->findChild<QObject *>(QStringLiteral("xai-wallet-replace"));
        QVERIFY(name && key && team && add && replace);
        QCOMPARE(key->property("echoMode").toInt(), 2);
        QCOMPARE(team->property("maximumLength").toInt(), 256);
        QVERIFY(name->setProperty("text", QStringLiteral("Work")));
        QVERIFY(key->setProperty("text", QStringLiteral("management")));
        QVERIFY(!add->property("enabled").toBool());
        QVERIFY(team->setProperty("text", QStringLiteral("team-a")));
        QVERIFY(add->property("enabled").toBool());
        accounts.failWrites = true;
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        QCOMPARE(team->property("text").toString(), QStringLiteral("team-a"));
        accounts.failWrites = false;
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        QCOMPARE(accounts.savedKey, QStringLiteral("management"));
        QCOMPARE(accounts.savedTeam, QStringLiteral("team-a"));
        QVERIFY(accounts.savedManagement.isEmpty());
        QCOMPARE(page->property("cfg_xaiAccountId").toString(), accounts.id);
        QVERIFY(key->property("text").toString().isEmpty());
        QVERIFY(team->property("text").toString().isEmpty());
        QVERIFY(key->setProperty("text", QStringLiteral("replacement")));
        QVERIFY(!replace->property("enabled").toBool());
        QVERIFY(team->setProperty("text", QStringLiteral("team-b")));
        QVERIFY(replace->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(replace, "clicked"));
        QCOMPARE(accounts.savedTeam, QStringLiteral("team-b"));
        QVERIFY(team->property("text").toString().isEmpty());
        QVERIFY(team->setProperty("text", QStringLiteral("draft")));
        QVERIFY(page->setProperty("cfg_xaiAccountId", QString{}));
        QVERIFY(team->property("text").toString().isEmpty());
        QVERIFY(team->setProperty("text", QStringLiteral("draft")));
        QVERIFY(accounts.setProperty("ready", false));
        QVERIFY(team->property("text").toString().isEmpty());
        page.reset();
        QCOMPARE(accounts.savedTeam,
                 QStringLiteral("team-b")); // Cancel preserves the wallet mutation.
    }

    void stagesZaiSelectionAndRequiresExplicitSelectors()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QQmlEngine engine;
        AccountUiStore accounts;
        QQmlComponent component(
            &engine,
            QUrl(QStringLiteral(
                "qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/ConfigWalletAccounts.qml")));
        QScopedPointer<QObject> page(component.createWithInitialProperties(
            {{"accountStore", QVariant::fromValue<QObject *>(&accounts)}}));
        QVERIFY2(page, qPrintable(component.errorString()));
        const auto field = [&](const char *suffix) {
            return page->findChild<QObject *>(QStringLiteral("zai-wallet-") +
                                              QLatin1String(suffix));
        };
        auto *name = field("name");
        auto *key = field("key");
        auto *region = field("region");
        auto *scope = field("scope");
        auto *org = field("organization");
        auto *project = field("project");
        auto *add = field("add");
        auto *replace = field("replace");
        QVERIFY(name && key && region && scope && org && project && add && replace);
        QVERIFY(page->property("cfg_zaiAccountId").toString().isEmpty());
        QCOMPARE(region->property("currentIndex").toInt(), -1);
        QCOMPARE(scope->property("currentIndex").toInt(), -1);
        name->setProperty("text", "Work");
        key->setProperty("text", "key");
        QVERIFY(!add->property("enabled").toBool());
        region->setProperty("currentIndex", 1);
        scope->setProperty("currentIndex", 1);
        QVERIFY(!add->property("enabled").toBool());
        org->setProperty("text", "org");
        project->setProperty("text", "project");
        QVERIFY(add->property("enabled").toBool());
        accounts.failWrites = true;
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        QCOMPARE(key->property("text").toString(), QStringLiteral("key"));
        QCOMPARE(region->property("currentIndex").toInt(), 1);
        accounts.failWrites = false;
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        QCOMPARE(accounts.savedZai, (QVariantMap{{"region", "bigmodel-cn"},
                                                 {"scope", "team"},
                                                 {"organizationId", "org"},
                                                 {"projectId", "project"}}));
        QCOMPARE(page->property("cfg_zaiAccountId").toString(), accounts.id);
        QVERIFY(key->property("text").toString().isEmpty());
        QVERIFY(org->property("text").toString().isEmpty());
        QCOMPARE(region->property("currentIndex").toInt(), -1);
        key->setProperty("text", "replacement");
        QVERIFY(!replace->property("enabled").toBool());
        region->setProperty("currentIndex", 0);
        scope->setProperty("currentIndex", 0);
        QVERIFY(replace->property("enabled").toBool());
        accounts.failWrites = true;
        QVERIFY(QMetaObject::invokeMethod(replace, "clicked"));
        QCOMPARE(key->property("text").toString(), QStringLiteral("replacement"));
        QCOMPARE(region->property("currentIndex").toInt(), 0);
        accounts.failWrites = false;
        QVERIFY(QMetaObject::invokeMethod(replace, "clicked"));
        QCOMPARE(accounts.savedZai, (QVariantMap{{"region", "global"}, {"scope", "personal"}}));
        org->setProperty("text", "draft");
        page->setProperty("cfg_zaiAccountId", QString{});
        QVERIFY(org->property("text").toString().isEmpty());
        project->setProperty("text", "draft");
        region->setProperty("currentIndex", 1);
        QVERIFY(accounts.setProperty("ready", false));
        QVERIFY(project->property("text").toString().isEmpty());
        QCOMPARE(region->property("currentIndex").toInt(), -1);
        page.reset(); // Cancel cannot undo the immediate wallet replacement.
        QCOMPARE(accounts.savedKey, QStringLiteral("replacement"));
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

    void displaysBankedResetsOnlyWhenAvailable()
    {
        QQmlEngine engine;
        for (const QString &file :
             {QStringLiteral("ProviderDetails.qml"), QStringLiteral("ProviderSummary.qml")}) {
            QQmlComponent component(
                &engine,
                QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/") + file));
            QVariantMap provider{{"id", "codex"}, {"bankedResets", 2}};
            QScopedPointer<QObject> object(component.createWithInitialProperties(
                {{"provider", provider}, {"width", 400}, {"height", 400}}));
            QVERIFY2(object, qPrintable(component.errorString()));
            auto *label = object->findChild<QQuickItem *>(QStringLiteral("bankedResets"));
            QVERIFY(label);
            QVERIFY(label->isVisible());
            QCOMPARE(label->property("text").toString(), QStringLiteral("Banked resets: 2"));
            provider.insert(QStringLiteral("bankedResets"), 1);
            QVERIFY(object->setProperty("provider", provider));
            QVERIFY(label->isVisible());
            QCOMPARE(label->property("text").toString(), QStringLiteral("Banked resets: 1"));
            provider.insert(QStringLiteral("bankedResets"), 0);
            QVERIFY(object->setProperty("provider", provider));
            QVERIFY(!label->isVisible());
            provider.remove(QStringLiteral("bankedResets"));
            QVERIFY(object->setProperty("provider", provider));
            QVERIFY(!label->isVisible());
        }
    }

    void usesFullWidthWithoutVerticalScrollbars()
    {
        QQmlEngine engine;
        for (const QString &file :
             {QStringLiteral("ProviderDetails.qml"), QStringLiteral("OverviewPage.qml")}) {
            QQmlComponent component(
                &engine,
                QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/org/kyaulabs/kodometer/") + file));
            QVariantMap properties{{"width", 240}, {"height", 80}};
            if (file == QStringLiteral("ProviderDetails.qml"))
                properties.insert(QStringLiteral("provider"), QVariantMap{{"id", "openrouter"}});
            QScopedPointer<QObject> object(component.createWithInitialProperties(properties));
            QVERIFY2(object, qPrintable(component.errorString()));
            auto *content = object->findChild<QQuickItem *>(QStringLiteral("pageContent"));
            auto *scrollbar = object->findChild<QQuickItem *>(QStringLiteral("pageScrollBar"));
            QVERIFY(content);
            QVERIFY(!scrollbar);
            QTRY_COMPARE(content->width(), 240);
            QVERIFY(object->setProperty("width", 480));
            QTRY_COMPARE(content->width(), 480);
        }
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
