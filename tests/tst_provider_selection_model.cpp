#include <codexbar/provider_selection_model.hpp>

#include <QSignalSpy>
#include <QtTest>

using CodexBar::ProviderSelectionModel;

namespace {

QVariantMap provider(const QString &id, const QString &name)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
    };
}

QVariantList providers(std::initializer_list<QVariantMap> entries)
{
    QVariantList result;
    for (const QVariantMap &entry : entries) {
        result.append(entry);
    }
    return result;
}

} // namespace

class ProviderSelectionModelTest final : public QObject
{
    Q_OBJECT

  private slots:
    void startsEmpty();
    void selectsOverviewForMultipleProviders();
    void selectsOnlyProvider();
    void selectsProviderById();
    void cyclesThroughTabs();
    void preservesAvailableSelection();
    void fallsBackWhenSelectionDisappears();
    void ignoresInvalidSelections();
    void avoidsRedundantSignals();
};

void ProviderSelectionModelTest::startsEmpty()
{
    ProviderSelectionModel model;

    QVERIFY(model.providers().isEmpty());
    QVERIFY(model.overviewSelected());
    QVERIFY(model.selectedProviderId().isEmpty());
    QVERIFY(model.selectedProvider().isEmpty());
    QCOMPARE(model.selectedTabIndex(), -1);
}

void ProviderSelectionModelTest::selectsOverviewForMultipleProviders()
{
    ProviderSelectionModel model;
    model.setProviders(providers({provider(QStringLiteral("codex"), QStringLiteral("Codex")),
                                  provider(QStringLiteral("claude"), QStringLiteral("Claude"))}));

    QVERIFY(model.overviewSelected());
    QCOMPARE(model.selectedTabIndex(), 0);
}

void ProviderSelectionModelTest::selectsOnlyProvider()
{
    ProviderSelectionModel model;
    model.setProviders(providers({provider(QStringLiteral("codex"), QStringLiteral("Codex"))}));

    QVERIFY(!model.overviewSelected());
    QCOMPARE(model.selectedProviderId(), QStringLiteral("codex"));
    QCOMPARE(model.selectedProvider().value(QStringLiteral("name")).toString(),
             QStringLiteral("Codex"));
    QCOMPARE(model.selectedTabIndex(), 0);
}

void ProviderSelectionModelTest::selectsProviderById()
{
    ProviderSelectionModel model;
    model.setProviders(providers({provider(QStringLiteral("codex"), QStringLiteral("Codex")),
                                  provider(QStringLiteral("claude"), QStringLiteral("Claude"))}));
    QSignalSpy selectionSpy(&model, &ProviderSelectionModel::selectionChanged);

    model.selectProvider(QStringLiteral("claude"));

    QCOMPARE(model.selectedProviderId(), QStringLiteral("claude"));
    QCOMPARE(model.selectedTabIndex(), 2);
    QCOMPARE(selectionSpy.count(), 1);
    model.selectOverview();
    QVERIFY(model.overviewSelected());
    QCOMPARE(selectionSpy.count(), 2);
}

void ProviderSelectionModelTest::cyclesThroughTabs()
{
    ProviderSelectionModel model;
    model.setProviders(providers({provider(QStringLiteral("codex"), QStringLiteral("Codex")),
                                  provider(QStringLiteral("claude"), QStringLiteral("Claude"))}));

    model.selectNext();
    QCOMPARE(model.selectedProviderId(), QStringLiteral("codex"));
    model.selectNext();
    QCOMPARE(model.selectedProviderId(), QStringLiteral("claude"));
    model.selectNext();
    QVERIFY(model.overviewSelected());

    model.selectPrevious();
    QCOMPARE(model.selectedProviderId(), QStringLiteral("claude"));
    model.selectPrevious();
    QCOMPARE(model.selectedProviderId(), QStringLiteral("codex"));
    model.selectPrevious();
    QVERIFY(model.overviewSelected());
}

void ProviderSelectionModelTest::preservesAvailableSelection()
{
    ProviderSelectionModel model;
    model.setProviders(providers({provider(QStringLiteral("codex"), QStringLiteral("Codex")),
                                  provider(QStringLiteral("claude"), QStringLiteral("Claude"))}));
    model.selectProvider(QStringLiteral("claude"));

    model.setProviders(providers({provider(QStringLiteral("claude"), QStringLiteral("Claude Max")),
                                  provider(QStringLiteral("gemini"), QStringLiteral("Gemini"))}));

    QCOMPARE(model.selectedProviderId(), QStringLiteral("claude"));
    QCOMPARE(model.selectedProvider().value(QStringLiteral("name")).toString(),
             QStringLiteral("Claude Max"));
}

void ProviderSelectionModelTest::fallsBackWhenSelectionDisappears()
{
    ProviderSelectionModel model;
    model.setProviders(providers({provider(QStringLiteral("codex"), QStringLiteral("Codex")),
                                  provider(QStringLiteral("claude"), QStringLiteral("Claude"))}));
    model.selectProvider(QStringLiteral("claude"));

    model.setProviders(providers({provider(QStringLiteral("codex"), QStringLiteral("Codex"))}));
    QCOMPARE(model.selectedProviderId(), QStringLiteral("codex"));

    model.setProviders({});
    QVERIFY(model.overviewSelected());
    QCOMPARE(model.selectedTabIndex(), -1);
}

void ProviderSelectionModelTest::ignoresInvalidSelections()
{
    ProviderSelectionModel model;
    model.setProviders(providers({provider(QStringLiteral("codex"), QStringLiteral("Codex"))}));
    QSignalSpy selectionSpy(&model, &ProviderSelectionModel::selectionChanged);

    model.selectProvider(QStringLiteral("missing"));
    model.selectOverview();
    model.selectNext();
    model.selectPrevious();

    QCOMPARE(model.selectedProviderId(), QStringLiteral("codex"));
    QCOMPARE(selectionSpy.count(), 0);
}

void ProviderSelectionModelTest::avoidsRedundantSignals()
{
    ProviderSelectionModel model;
    const QVariantList values =
        providers({provider(QStringLiteral("codex"), QStringLiteral("Codex")),
                   provider(QStringLiteral("claude"), QStringLiteral("Claude"))});
    QSignalSpy providersSpy(&model, &ProviderSelectionModel::providersChanged);
    QSignalSpy selectionSpy(&model, &ProviderSelectionModel::selectionChanged);

    model.setProviders(values);
    model.setProviders(values);
    model.selectOverview();

    QCOMPARE(providersSpy.count(), 1);
    QCOMPARE(selectionSpy.count(), 0);
}

QTEST_GUILESS_MAIN(ProviderSelectionModelTest)

#include "tst_provider_selection_model.moc"
