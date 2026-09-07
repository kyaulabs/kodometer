#include <kodometer/provider_actions.hpp>

#include <QDesktopServices>
#include <QtTest>

using Kodometer::ProviderActions;

class ProviderActionsTest final : public QObject
{
    Q_OBJECT

  private slots:
    void destinations_data()
    {
        QTest::addColumn<QString>("provider");
        QTest::addColumn<QString>("region");
        QTest::addColumn<QString>("dashboard");
        QTest::addColumn<QString>("documentation");
        QTest::newRow("codex") << QStringLiteral("codex") << QString()
                               << QStringLiteral("https://chatgpt.com/codex/settings/usage")
                               << QStringLiteral("https://developers.openai.com/codex/");
        QTest::newRow("claude") << QStringLiteral("claude") << QString()
                                << QStringLiteral("https://claude.ai/settings/usage")
                                << QStringLiteral("https://code.claude.com/docs/en/overview");
        QTest::newRow("gemini") << QStringLiteral("gemini") << QString()
                                << QStringLiteral("https://console.cloud.google.com/")
                                << QStringLiteral(
                                       "https://cloud.google.com/gemini/docs/codeassist/overview");
        QTest::newRow("xai") << QStringLiteral("xai") << QString()
                             << QStringLiteral("https://console.x.ai/")
                             << QStringLiteral("https://docs.x.ai/");
        QTest::newRow("kimi") << QStringLiteral("kimi") << QString()
                              << QStringLiteral("https://www.kimi.com/code/console")
                              << QStringLiteral("https://www.kimi.com/code/docs/");
        QTest::newRow("deepseek") << QStringLiteral("deepseek") << QString()
                                  << QStringLiteral("https://platform.deepseek.com/usage")
                                  << QStringLiteral("https://api-docs.deepseek.com/");
        QTest::newRow("openrouter") << QStringLiteral("openrouter") << QString()
                                    << QStringLiteral("https://openrouter.ai/activity")
                                    << QStringLiteral("https://openrouter.ai/docs/overview");
        QTest::newRow("zai-global")
            << QStringLiteral("zai") << QStringLiteral("global")
            << QStringLiteral("https://z.ai/manage-apikey/coding-plan/personal/my-plan")
            << QStringLiteral("https://docs.z.ai/");
        QTest::newRow("zai-cn") << QStringLiteral("zai") << QStringLiteral("bigmodel-cn")
                                << QStringLiteral("https://bigmodel.cn/")
                                << QStringLiteral("https://docs.bigmodel.cn/");
    }

    void destinations()
    {
        QFETCH(QString, provider);
        QFETCH(QString, region);
        QFETCH(QString, dashboard);
        QFETCH(QString, documentation);
        QList<QUrl> opened;
        ProviderActions actions([&opened](const QUrl &url) {
            opened.append(url);
            return true;
        });
        QVERIFY(actions.actions().isEmpty());
        actions.setProviderId(provider);
        actions.setRegion(region);
        QCOMPARE(actions.providerId(), provider);
        QCOMPARE(actions.region(), region);
        const QVariantList entries = actions.actions();
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).toMap().value("id").toString(), QStringLiteral("dashboard"));
        QCOMPARE(entries.at(1).toMap().value("id").toString(), QStringLiteral("documentation"));
        QVERIFY(opened.isEmpty()); // Reading presentation must not launch anything.
        for (const QVariant &entry : entries) {
            const QVariantMap row = entry.toMap();
            QVERIFY(!row.value("label").toString().isEmpty());
            QVERIFY(actions.open(row.value("id").toString()));
            const QUrl url = opened.last();
            QCOMPARE(row.value("url").toUrl(), url);
            QCOMPARE(url.scheme(), QStringLiteral("https"));
            QVERIFY(url.userInfo().isEmpty());
            QVERIFY(url.query().isEmpty());
            QVERIFY(url.fragment().isEmpty());
            QVERIFY(actions.error().isEmpty());
        }
        QCOMPARE(opened, (QList<QUrl>{QUrl(dashboard), QUrl(documentation)}));
    }

    void rejectsUntrustedTargets()
    {
        int launches = 0;
        ProviderActions actions([&launches](const QUrl &) {
            ++launches;
            return true;
        });
        actions.setProviderId(QStringLiteral("codex"));
        for (const QString &id :
             {QString(), QStringLiteral("https://evil.test"), QStringLiteral("file:///tmp/run"),
              QStringLiteral("Dashboard"), QStringLiteral("dashboard?token=secret")}) {
            QVERIFY(!actions.open(id));
            QCOMPARE(actions.error(), QStringLiteral("This provider action is unavailable"));
        }
        actions.setProviderId(QStringLiteral("https://evil.test"));
        QVERIFY(actions.actions().isEmpty());
        QVERIFY(!actions.open(QStringLiteral("dashboard")));
        actions.setProviderId(QStringLiteral("zai"));
        for (const QString &region :
             {QString(), QStringLiteral("BigModel CN"), QStringLiteral("https://evil.test"),
              QStringLiteral("GLOBAL")}) {
            actions.setRegion(region);
            QVERIFY(actions.actions().isEmpty());
            QVERIFY(!actions.open(QStringLiteral("documentation")));
        }
        QCOMPARE(launches, 0);
    }

    void reportsLaunchFailuresAndClearsContext()
    {
        bool accepted = false;
        ProviderActions actions([&accepted](const QUrl &) { return accepted; });
        QSignalSpy context(&actions, &ProviderActions::contextChanged);
        QSignalSpy errors(&actions, &ProviderActions::errorChanged);
        actions.setProviderId(QStringLiteral("codex"));
        actions.setProviderId(QStringLiteral("codex"));
        QCOMPARE(context.count(), 1);
        QVERIFY(!actions.open(QStringLiteral("dashboard")));
        QCOMPARE(actions.error(),
                 QStringLiteral("Could not open the provider page in your browser"));
        QVERIFY(!actions.open(QStringLiteral("dashboard")));
        QCOMPARE(errors.count(), 1);
        accepted = true;
        QVERIFY(actions.open(QStringLiteral("dashboard")));
        QVERIFY(actions.error().isEmpty());
        QVERIFY(!actions.open(QStringLiteral("unknown")));
        actions.setProviderId(QStringLiteral("zai"));
        QVERIFY(actions.error().isEmpty());
        QVERIFY(!actions.open(QStringLiteral("dashboard")));
        actions.setRegion(QStringLiteral("global"));
        QVERIFY(actions.error().isEmpty());
        actions.setRegion(QStringLiteral("global"));
        QCOMPARE(context.count(), 3);
        ProviderActions missingOpener(ProviderActions::UrlOpener{});
        missingOpener.setProviderId(QStringLiteral("codex"));
        QVERIFY(!missingOpener.open(QStringLiteral("dashboard")));
    }

    void usesNativeDesktopIntegration()
    {
        QDesktopServices::setUrlHandler(QStringLiteral("https"), this, "recordUrl");
        ProviderActions actions;
        actions.setProviderId(QStringLiteral("codex"));
        const bool accepted = actions.open(QStringLiteral("dashboard"));
        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
        QVERIFY(accepted);
        QCOMPARE(m_url, QUrl(QStringLiteral("https://chatgpt.com/codex/settings/usage")));
    }

    void recordUrl(const QUrl &url)
    {
        m_url = url;
    }

  private:
    QUrl m_url;
};

QTEST_MAIN(ProviderActionsTest)
#include "tst_provider_actions.moc"
