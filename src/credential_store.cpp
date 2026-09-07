#include <kodometer/credential_store.hpp>

namespace Kodometer {
namespace {

bool validSecret(const QString &secret)
{
    return !secret.isEmpty() && secret.size() <= CredentialStore::MaximumSecretSize &&
           !secret.contains(QLatin1Char('\n')) && !secret.contains(QLatin1Char('\r'));
}

} // namespace

CredentialStore::CredentialStore(CredentialBackend *backend, QObject *parent)
    : QObject(parent), m_backend(backend), m_accounts(backend, this)
{
    Q_ASSERT(m_backend != nullptr); // GCOVR_EXCL_BR_LINE -- constructor invariant
    if (m_backend->parent() == nullptr) {
        m_backend->setParent(this);
    }
    connect(m_backend, &CredentialBackend::openFinished, this,
            &CredentialStore::handleOpenFinished);
    connect(m_backend, &CredentialBackend::changed, this, [this] {
        if (m_backendOpen) {
            (void)m_accounts.reload();
            (void)reload();
        }
    });
    connect(m_backend, &CredentialBackend::closed, this, &CredentialStore::handleClosed);
}

WalletAccounts *CredentialStore::accounts() noexcept
{
    return &m_accounts;
}

bool CredentialStore::ready() const noexcept
{
    return m_ready;
}

bool CredentialStore::busy() const noexcept
{
    return m_busy;
}

QString CredentialStore::error() const
{
    return m_error;
}

QStringList CredentialStore::configuredKeys() const
{
    QStringList configured;
    for (const QString &key : supportedKeys()) {
        if (m_secrets.contains(key)) {
            configured.append(key);
        }
    }
    return configured;
}

QMap<QString, QString> CredentialStore::secrets() const
{
    return m_secrets;
}

QStringList CredentialStore::supportedKeys()
{
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    return {
        QStringLiteral("DEEPSEEK_API_KEY"),       QStringLiteral("KIMI_CODE_API_KEY"),
        QStringLiteral("OPENROUTER_API_KEY"),     QStringLiteral("OPENROUTER_MANAGEMENT_API_KEY"),
        QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("Z_AI_API_KEY"),
    };
    // GCOVR_EXCL_BR_STOP
}

void CredentialStore::open()
{
    if (m_busy) {
        return;
    }
    if (m_ready) {
        (void)m_accounts.reload();
        (void)reload();
        return;
    }
    const quint64 session = ++m_sessionRevision;
    ++m_readRevision;
    setBusy(true);
    if (session != m_sessionRevision)
        return;
    setError({});
    if (session == m_sessionRevision)
        m_backend->open();
}

bool CredentialStore::hasSecret(const QString &key) const
{
    return supportedKey(key) && m_secrets.contains(key);
}

bool CredentialStore::saveSecret(const QString &key, const QString &value)
{
    if (!supportedKey(key)) {
        setError(QStringLiteral("Credential key is not supported"));
        return false;
    }
    if (!m_ready) {
        setError(QStringLiteral("KWallet is not ready"));
        return false;
    }
    const QString secret = value.trimmed();
    if (secret.isEmpty()) {
        setError(QStringLiteral("Credential value is empty"));
        return false;
    }
    if (secret.size() > MaximumSecretSize) {
        setError(QStringLiteral("Credential value exceeds the 64 KiB limit"));
        return false;
    }
    if (!validSecret(secret)) {
        setError(QStringLiteral("Credential value contains invalid characters"));
        return false;
    }

    QString backendError;
    const quint64 session = m_sessionRevision;
    const bool stored = m_backend->writeSecret(key, secret, &backendError);
    if (session != m_sessionRevision)
        return false;
    if (!stored) {
        if (backendError.isEmpty()) {
            backendError = QStringLiteral("KWallet could not store the credential");
        }
        setError(backendError);
        return false;
    }
    return reload();
}

bool CredentialStore::removeSecret(const QString &key)
{
    if (!supportedKey(key)) {
        setError(QStringLiteral("Credential key is not supported"));
        return false;
    }
    if (!m_ready) {
        setError(QStringLiteral("KWallet is not ready"));
        return false;
    }

    QString backendError;
    const quint64 session = m_sessionRevision;
    const bool removed = m_backend->removeSecret(key, &backendError);
    if (session != m_sessionRevision)
        return false;
    if (!removed) {
        if (backendError.isEmpty()) {
            backendError = QStringLiteral("KWallet could not remove the credential");
        }
        setError(backendError);
        return false;
    }
    return reload();
}

bool CredentialStore::supportedKey(const QString &key)
{
    return supportedKeys().contains(key);
}

bool CredentialStore::reload()
{
    if (!m_backendOpen)
        return false;
    const quint64 revision = ++m_readRevision;
    QString backendError;
    const auto loaded = m_backend->readSecrets(supportedKeys(), &backendError);
    // Synchronous D-Bus reads can dispatch nested close/update notifications.
    if (revision != m_readRevision)
        return m_ready;
    if (!loaded) {
        if (backendError.isEmpty()) {
            backendError = QStringLiteral("KWallet credentials could not be read");
        }
        publishState({}, false, backendError);
        return m_ready;
    }
    QMap<QString, QString> normalized;
    for (auto iterator = loaded->cbegin(); iterator != loaded->cend(); ++iterator) {
        const QString secret = iterator.value().trimmed();
        if (!validSecret(secret)) {
            const QString error =
                QStringLiteral("KWallet contains an invalid credential for %1").arg(iterator.key());
            publishState({}, false, error);
            return m_ready;
        }
        normalized.insert(iterator.key(), secret);
    }
    publishState(normalized, true, {});
    return m_ready;
}

void CredentialStore::handleOpenFinished(bool success, const QString &error)
{
    if (!m_busy)
        return; // An abandoned asynchronous open cannot restore credentials after closure.
    const quint64 session = m_sessionRevision;
    m_backendOpen = success;
    if (!success) {
        QString message = error;
        if (message.isEmpty()) {
            message = QStringLiteral("KWallet could not be opened");
        }
        publishState({}, false, message);
    }
    else {
        m_accounts.setAvailable(true);
        if (session == m_sessionRevision)
            (void)reload();
    }
    if (session == m_sessionRevision)
        setBusy(false);
}

void CredentialStore::handleClosed()
{
    const quint64 session = ++m_sessionRevision;
    m_backendOpen = false;
    publishState({}, false, QStringLiteral("KWallet was closed"));
    if (session == m_sessionRevision)
        setBusy(false);
}

void CredentialStore::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void CredentialStore::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

void CredentialStore::publishState(const QMap<QString, QString> &secrets, bool ready,
                                   const QString &error)
{
    ++m_readRevision;
    const bool secretsDiffer = m_secrets != secrets;
    const bool readyDiffers = m_ready != ready;
    const bool errorDiffers = m_error != error;
    const QStringList previousKeys = configuredKeys();
    m_secrets = secrets;
    m_ready = ready;
    m_error = error;
    const bool keysDiffer = configuredKeys() != previousKeys;
    // Commit Default state before named-account callbacks can start another refresh.
    // Emit notifications only after both registries have dropped closed-wallet data.
    if (!m_backendOpen)
        m_accounts.setAvailable(false);
    if (secretsDiffer)
        emit secretsChanged();
    if (keysDiffer)
        emit configuredKeysChanged();
    if (readyDiffers)
        emit readyChanged();
    if (errorDiffers)
        emit errorChanged();
}

} // namespace Kodometer
