#include <codexbar/provider_selection_model.hpp>

#include <utility>

namespace CodexBar {

ProviderSelectionModel::ProviderSelectionModel(QObject *parent) : QObject(parent) {}

QVariantList ProviderSelectionModel::providers() const
{
    return m_providers;
}

void ProviderSelectionModel::setProviders(const QVariantList &providers)
{
    if (m_providers == providers) {
        return;
    }

    const QString previousSelection = m_selectedProviderId;
    m_providers = providers;
    emit providersChanged();

    QString nextSelection;
    if (m_providers.size() == 1) {
        nextSelection = providerIdAt(0);
    }
    else if (!previousSelection.isEmpty() && providerIndex(previousSelection) >= 0) {
        nextSelection = previousSelection;
    }

    const bool selectedProviderChanged =
        !nextSelection.isEmpty() && nextSelection == previousSelection;
    if (nextSelection != previousSelection) {
        m_selectedProviderId = nextSelection;
        emit selectionChanged();
    }
    else if (selectedProviderChanged) {
        emit selectionChanged();
    }
}

QString ProviderSelectionModel::selectedProviderId() const
{
    return m_selectedProviderId;
}

QVariantMap ProviderSelectionModel::selectedProvider() const
{
    const qsizetype index = providerIndex(m_selectedProviderId);
    if (index < 0) {
        return {};
    }
    return m_providers.at(index).toMap();
}

bool ProviderSelectionModel::overviewSelected() const noexcept
{
    return m_selectedProviderId.isEmpty();
}

int ProviderSelectionModel::selectedTabIndex() const
{
    if (m_providers.isEmpty()) {
        return -1;
    }
    if (overviewSelected()) {
        return 0;
    }

    const qsizetype index = providerIndex(m_selectedProviderId);
    return static_cast<int>(index + (m_providers.size() > 1 ? 1 : 0));
}

void ProviderSelectionModel::selectOverview()
{
    if (m_providers.size() <= 1) {
        return;
    }
    setSelection({});
}

void ProviderSelectionModel::selectProvider(const QString &providerId)
{
    if (providerIndex(providerId) < 0) {
        return;
    }
    setSelection(providerId);
}

void ProviderSelectionModel::selectNext()
{
    const qsizetype tabCount = m_providers.size() + (m_providers.size() > 1 ? 1 : 0);
    if (tabCount <= 1) {
        return;
    }
    selectTab((selectedTabIndex() + 1) % tabCount);
}

void ProviderSelectionModel::selectPrevious()
{
    const qsizetype tabCount = m_providers.size() + (m_providers.size() > 1 ? 1 : 0);
    if (tabCount <= 1) {
        return;
    }
    selectTab((selectedTabIndex() + tabCount - 1) % tabCount);
}

qsizetype ProviderSelectionModel::providerIndex(const QString &providerId) const
{
    if (providerId.isEmpty()) {
        return -1;
    }
    for (qsizetype index = 0; index < m_providers.size(); ++index) {
        if (providerIdAt(index) == providerId) {
            return index;
        }
    }
    return -1;
}

QString ProviderSelectionModel::providerIdAt(qsizetype index) const
{
    return m_providers.at(index).toMap().value(QStringLiteral("id")).toString();
}

void ProviderSelectionModel::setSelection(const QString &providerId)
{
    if (m_selectedProviderId == providerId) {
        return;
    }
    m_selectedProviderId = providerId;
    emit selectionChanged();
}

void ProviderSelectionModel::selectTab(qsizetype tabIndex)
{
    const bool hasOverview = m_providers.size() > 1;
    if (hasOverview && tabIndex == 0) {
        setSelection({});
        return;
    }
    setSelection(providerIdAt(tabIndex - (hasOverview ? 1 : 0)));
}

} // namespace CodexBar
