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
    : QObject(parent), m_refreshTimer(this)
{
    m_refreshTimer.setObjectName(QStringLiteral("refreshTimer"));
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &UsageController::refresh);
    for (ProviderAdapter *adapter : adapters) {
        registerAdapter(adapter);
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
        if (!m_disabledProviders.contains(adapter->providerId())) {
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
    if (!m_disabledProviders.contains(adapter->providerId())) {
        m_providerSnapshots.insert(adapter->providerId(), provider);
        m_anySuccess = true;
        rebuildProviders();
        emit providerRefreshed(provider);
    }
    finishAdapter();
}

void UsageController::adapterFailed(ProviderAdapter *adapter, const QString &error)
{
    if (!m_pendingAdapters.remove(adapter)) {
        return;
    }
    m_refreshErrors.insert(adapter->providerId(), error);
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
        const auto iterator = m_providerSnapshots.constFind(adapter->providerId());
        if (iterator != m_providerSnapshots.cend()) {
            providers.append(*iterator);
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
