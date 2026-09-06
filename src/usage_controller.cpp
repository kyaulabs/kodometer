#include <kodometer/usage_controller.hpp>

#include <kodometer/claude_provider_adapter.hpp>
#include <kodometer/codex_provider_adapter.hpp>
#include <kodometer/deepseek_provider_adapter.hpp>
#include <kodometer/gemini_provider_adapter.hpp>
#include <kodometer/kimi_provider_adapter.hpp>
#include <kodometer/openrouter_provider_adapter.hpp>
#include <kodometer/xai_provider_adapter.hpp>
#include <kodometer/zai_provider_adapter.hpp>

#include <QDateTime>

#include <algorithm>

namespace Kodometer {
namespace {

QString displayName(const QString &providerId)
{
    if (providerId.isEmpty()) {
        return QStringLiteral("Provider");
    }
    QString name = providerId;
    name[0] = name.at(0).toUpper();
    return name;
}

} // namespace

// GCOVR_EXCL_START -- composition root has only Qt allocation branches
UsageController::UsageController(QObject *parent)
    : UsageController(QList<ProviderAdapter *>{new CodexProviderAdapter, new ClaudeProviderAdapter,
                                               new GeminiProviderAdapter, new XaiProviderAdapter,
                                               new KimiProviderAdapter, new DeepSeekProviderAdapter,
                                               new ZaiProviderAdapter,
                                               new OpenRouterProviderAdapter},
                      parent)
{}
// GCOVR_EXCL_STOP

UsageController::UsageController(const QList<ProviderAdapter *> &adapters, QObject *parent)
    : QObject(parent), m_profiles(this), m_refreshTimer(this)
{
    m_refreshTimer.setObjectName(QStringLiteral("refreshTimer"));
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &UsageController::refresh);
    connect(&m_profiles, &OAuthProfiles::configurationChanged, this,
            &UsageController::profileSettingsChanged);
    for (ProviderAdapter *adapter : adapters) {
        registerAdapter(adapter);
    }
}

OAuthProfiles *UsageController::profiles() noexcept
{
    return &m_profiles;
}

void UsageController::profileSettingsChanged()
{
    QStringList changed;
    for (ProviderAdapter *adapter : std::as_const(m_adapters)) {
        const QString id = adapter->providerId();
        const QString context = m_profiles.contextKey(id);
        if (m_profileContexts.value(id) != context) {
            m_profileContexts.insert(id, context);
            ++m_profileRevisions[id];
            m_providerSnapshots.remove(id);
            m_refreshErrors.remove(id);
            changed.append(id);
        }
    }
    // Remove all changed contexts together, before publishing any new profile labels.
    rebuildProviders();
    rebuildErrors();
    for (const QString &id : changed) {
        emit providerContextChanged(id);
    }
    if (m_started && !changed.isEmpty()) {
        queueProfileRefresh();
    }
}

void UsageController::queueProfileRefresh()
{
    if (m_busy) {
        m_refreshAfterCurrent = true;
    }
    else if (!m_profileRefreshQueued) {
        m_profileRefreshQueued = true;
        QMetaObject::invokeMethod(
            this,
            [this] {
                m_profileRefreshQueued = false;
                if (m_busy) {
                    m_refreshAfterCurrent = true;
                }
                else {
                    refresh();
                }
            },
            Qt::QueuedConnection);
    }
}

bool UsageController::busy() const noexcept
{
    return m_busy;
}

QString UsageController::error() const
{
    return m_error;
}

QVariantMap UsageController::snapshot() const
{
    return m_snapshot;
}

QVariantList UsageController::providers() const
{
    return m_providers;
}

CredentialStore *UsageController::credentialStore() const noexcept
{
    return m_credentialStore;
}

void UsageController::setCredentialStore(CredentialStore *store)
{
    if (m_credentialStore == store) {
        return;
    }
    if (m_credentialStore != nullptr) {
        disconnect(m_credentialStore, nullptr, this, nullptr);
    }
    m_credentialStore = store;
    applyCredentialOverrides();
    if (m_credentialStore != nullptr) {
        connect(m_credentialStore, &CredentialStore::secretsChanged, this,
                &UsageController::credentialsChanged);
        connect(m_credentialStore, &QObject::destroyed, this, [this] {
            m_credentialStore = nullptr;
            applyCredentialOverrides();
            emit credentialStoreChanged();
        });
    }
    emit credentialStoreChanged();
}

bool UsageController::autoRefresh() const noexcept
{
    return m_autoRefresh;
}

int UsageController::refreshIntervalMinutes() const noexcept
{
    return m_refreshIntervalMinutes;
}

QStringList UsageController::disabledProviders() const
{
    return m_disabledProviders;
}

void UsageController::setAutoRefresh(bool enabled)
{
    if (m_autoRefresh == enabled) {
        return;
    }
    m_autoRefresh = enabled;
    scheduleRefresh();
    emit refreshSettingsChanged();
}

void UsageController::setRefreshIntervalMinutes(int minutes)
{
    const int bounded = std::clamp(minutes, 1, 1440);
    if (m_refreshIntervalMinutes == bounded) {
        return;
    }
    m_refreshIntervalMinutes = bounded;
    scheduleRefresh();
    emit refreshSettingsChanged();
}

void UsageController::setDisabledProviders(const QStringList &providers)
{
    QStringList normalized = providers;
    normalized.removeDuplicates();
    normalized.sort();
    if (m_disabledProviders == normalized) {
        return;
    }
    m_disabledProviders = normalized;
    rebuildProviders();
    rebuildErrors();
    emit disabledProvidersChanged();
    if (m_started) {
        if (m_busy) {
            m_refreshAfterCurrent = true;
        }
        else {
            refresh();
        }
    }
}

QList<ProviderAdapter *> UsageController::enabledAdapters() const
{
    QList<ProviderAdapter *> enabled;
    for (ProviderAdapter *adapter : m_adapters) {
        const QString id = adapter->providerId();
        const bool profileAllowed = !OAuthProfiles::supportsProvider(id) || m_profiles.valid();
        if (!m_disabledProviders.contains(id) && profileAllowed) {
            enabled.append(adapter);
        }
    }
    return enabled;
}

void UsageController::scheduleRefresh()
{
    m_refreshTimer.stop();
    if (m_started && m_autoRefresh && !m_busy && !enabledAdapters().isEmpty()) {
        m_refreshTimer.start(m_refreshIntervalMinutes * 60000);
    }
}

void UsageController::refresh()
{
    if (m_busy) {
        return;
    }
    m_started = true;
    m_refreshTimer.stop();
    m_refreshErrors.clear();
    m_anySuccess = false;
    setError({});
    const QList<ProviderAdapter *> active = enabledAdapters();
    if (active.isEmpty()) {
        emit refreshFinished(true);
        return;
    }

    m_pendingAdapters = QSet<ProviderAdapter *>(active.cbegin(), active.cend());
    m_pendingProfileRevisions.clear();
    for (ProviderAdapter *adapter : active) {
        const QString id = adapter->providerId();
        m_pendingProfileRevisions.insert(adapter, m_profileRevisions.value(id));
        if (OAuthProfiles::supportsProvider(id)) {
            adapter->setProfileDirectory(m_profiles.selectedDirectory(id));
        }
    }
    setBusy(true);
    for (ProviderAdapter *adapter : active) {
        adapter->refresh();
    }
}

void UsageController::registerAdapter(ProviderAdapter *adapter)
{
    if (adapter == nullptr) {
        return;
    }
    if (adapter->parent() == nullptr) {
        adapter->setParent(this);
    }
    m_adapters.append(adapter);
    m_profileContexts.insert(adapter->providerId(), m_profiles.contextKey(adapter->providerId()));
    connect(adapter, &ProviderAdapter::refreshSucceeded, this,
            [this, adapter](const QVariantMap &provider) { adapterSucceeded(adapter, provider); });
    connect(adapter, &ProviderAdapter::refreshFailed, this,
            [this, adapter](const QString &error) { adapterFailed(adapter, error); });
}

void UsageController::adapterSucceeded(ProviderAdapter *adapter, const QVariantMap &provider)
{
    if (!m_pendingAdapters.remove(adapter)) {
        return;
    }
    const QString id = adapter->providerId();
    const quint64 revision = m_pendingProfileRevisions.take(adapter);
    const bool current = revision == m_profileRevisions.value(id);
    if (!m_disabledProviders.contains(id) && current) {
        m_providerSnapshots.insert(id, provider);
        m_anySuccess = true;
        rebuildProviders();
        // Presentation signals can synchronously change selection or disable the provider.
        if (revision == m_profileRevisions.value(id) && !m_disabledProviders.contains(id)) {
            emit providerRefreshed(provider);
        }
    }
    finishAdapter();
}

void UsageController::adapterFailed(ProviderAdapter *adapter, const QString &error)
{
    if (!m_pendingAdapters.remove(adapter)) {
        return;
    }
    if (m_pendingProfileRevisions.take(adapter) ==
        m_profileRevisions.value(adapter->providerId())) {
        m_refreshErrors.insert(adapter->providerId(), error);
    }
    finishAdapter();
}

void UsageController::finishAdapter()
{
    if (!m_pendingAdapters.isEmpty()) {
        return;
    }

    if (m_anySuccess) {
        // GCOVR_EXCL_BR_START -- Qt container allocation branches
        m_snapshot = {
            {QStringLiteral("generatedAt"),
             QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {QStringLiteral("providers"), m_providers},
        };
        // GCOVR_EXCL_BR_STOP
        emit snapshotChanged();
    }
    rebuildErrors();
    setBusy(false);
    scheduleRefresh();
    emit refreshFinished(m_error.isEmpty());
    if (m_refreshAfterCurrent) {
        m_refreshAfterCurrent = false;
        QMetaObject::invokeMethod(this, &UsageController::refresh, Qt::QueuedConnection);
    }
}

void UsageController::rebuildProviders()
{
    QVariantList providers;
    providers.reserve(m_adapters.size());
    for (const ProviderAdapter *adapter : enabledAdapters()) {
        const QString id = adapter->providerId();
        const QString selectedId = m_profiles.selectedId(id);
        const auto iterator = m_providerSnapshots.constFind(id);
        if (iterator != m_providerSnapshots.cend()) {
            QVariantMap provider = *iterator;
            if (OAuthProfiles::supportsProvider(id) && selectedId != QLatin1String("default")) {
                provider.insert(QStringLiteral("profileName"), m_profiles.selectedName(id));
            }
            providers.append(provider);
        }
    }
    if (m_providers == providers) {
        return;
    }
    m_providers = providers;
    emit providersChanged();
    if (!m_snapshot.isEmpty()) {
        m_snapshot.insert(QStringLiteral("providers"), m_providers);
        emit snapshotChanged();
    }
}

void UsageController::rebuildErrors()
{
    QStringList errors;
    for (const ProviderAdapter *adapter : enabledAdapters()) {
        const auto error = m_refreshErrors.constFind(adapter->providerId());
        if (error != m_refreshErrors.cend()) {
            errors.append(QStringLiteral("%1: %2").arg(displayName(adapter->providerId()), *error));
        }
    }
    setError(errors.join(QLatin1Char('\n')));
}

void UsageController::applyCredentialOverrides()
{
    const QMap<QString, QString> secrets =
        m_credentialStore == nullptr ? QMap<QString, QString>{} : m_credentialStore->secrets();
    for (ProviderAdapter *adapter : std::as_const(m_adapters)) {
        adapter->setCredentialOverrides(secrets);
    }
}

void UsageController::credentialsChanged()
{
    applyCredentialOverrides();
    if (m_busy) {
        m_refreshAfterCurrent = true;
    }
    else {
        refresh();
    }
}

void UsageController::setBusy(bool busy)
{
    if (m_busy == busy) { // GCOVR_EXCL_BR_LINE -- callers only transition state
        return;           // GCOVR_EXCL_LINE
    }
    m_busy = busy;
    emit busyChanged();
}

void UsageController::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
