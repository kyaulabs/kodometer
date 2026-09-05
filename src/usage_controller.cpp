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
    : QObject(parent)
{
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

void UsageController::refresh()
{
    if (m_busy) {
        return;
    }
    if (m_adapters.isEmpty()) {
        emit refreshFinished(true);
        return;
    }

    m_refreshErrors.clear();
    m_pendingAdapters = m_adapters.size();
    m_anySuccess = false;
    setError({});
    setBusy(true);
    for (ProviderAdapter *adapter : m_adapters) {
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
    m_providerSnapshots.insert(adapter->providerId(), provider);
    m_anySuccess = true;
    rebuildProviders();
    finishAdapter();
}

void UsageController::adapterFailed(ProviderAdapter *adapter, const QString &error)
{
    m_refreshErrors.append(QStringLiteral("%1: %2").arg(displayName(adapter->providerId()), error));
    finishAdapter();
}

void UsageController::finishAdapter()
{
    --m_pendingAdapters;
    if (m_pendingAdapters > 0) {
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
    setError(m_refreshErrors.join(QLatin1Char('\n')));
    setBusy(false);
    emit refreshFinished(m_refreshErrors.isEmpty());
    if (m_refreshAfterCurrent) {
        m_refreshAfterCurrent = false;
        QMetaObject::invokeMethod(this, &UsageController::refresh, Qt::QueuedConnection);
    }
}

void UsageController::rebuildProviders()
{
    QVariantList providers;
    providers.reserve(m_adapters.size());
    for (const ProviderAdapter *adapter : std::as_const(m_adapters)) {
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
