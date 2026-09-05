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
    : QObject(parent), m_backend(backend)
{
    Q_ASSERT(m_backend != nullptr); // GCOVR_EXCL_BR_LINE -- constructor invariant
    if (m_backend->parent() == nullptr) {
        m_backend->setParent(this);
    }
    connect(m_backend, &CredentialBackend::openFinished, this,
            &CredentialStore::handleOpenFinished);
    connect(m_backend, &CredentialBackend::changed, this, [this] {
        if (m_ready) {
            (void)reload();
        }
    });
    connect(m_backend, &CredentialBackend::closed, this, &CredentialStore::handleClosed);
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
    if (m_ready || m_busy) {
        return;
    }
    setError({});
    setBusy(true);
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
    if (!m_backend->writeSecret(key, secret, &backendError)) {
        if (backendError.isEmpty()) {
            backendError = QStringLiteral("KWallet could not store the credential");
        }
        setError(backendError);
        return false;
    }
    if (!reload()) {
        return false;
    }
    setError({});
    return true;
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
    if (!m_backend->removeSecret(key, &backendError)) {
        if (backendError.isEmpty()) {
            backendError = QStringLiteral("KWallet could not remove the credential");
        }
        setError(backendError);
        return false;
    }
    if (!reload()) {
        return false;
    }
    setError({});
    return true;
}

bool CredentialStore::supportedKey(const QString &key)
{
    return supportedKeys().contains(key);
}

bool CredentialStore::reload()
{
    QString backendError;
    const auto loaded = m_backend->readSecrets(supportedKeys(), &backendError);
    if (!loaded) {
        if (backendError.isEmpty()) {
            backendError = QStringLiteral("KWallet credentials could not be read");
        }
        setError(backendError);
        return false;
    }
    QMap<QString, QString> normalized;
    for (auto iterator = loaded->cbegin(); iterator != loaded->cend(); ++iterator) {
        const QString secret = iterator.value().trimmed();
        if (!validSecret(secret)) {
            setError(QStringLiteral("KWallet contains an invalid credential for %1")
                         .arg(iterator.key()));
            return false;
        }
        normalized.insert(iterator.key(), secret);
    }
    setSecrets(normalized);
    return true;
}

void CredentialStore::handleOpenFinished(bool success, const QString &error)
{
    setBusy(false);
    if (!success) {
        setReady(false);
        QString message = error;
        if (message.isEmpty()) {
            message = QStringLiteral("KWallet could not be opened");
        }
        setError(message);
        return;
    }
    if (!reload()) {
        setReady(false);
        return;
    }
    setReady(true);
    setError({});
}

void CredentialStore::handleClosed()
{
    setBusy(false);
    setReady(false);
    setSecrets({});
    setError(QStringLiteral("KWallet was closed"));
}

void CredentialStore::setReady(bool ready)
{
    if (m_ready == ready) {
        return;
    }
    m_ready = ready;
    emit readyChanged();
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

void CredentialStore::setSecrets(const QMap<QString, QString> &secrets)
{
    if (m_secrets == secrets) {
        return;
    }
    const QStringList previousKeys = configuredKeys();
    m_secrets = secrets;
    if (configuredKeys() != previousKeys) {
        emit configuredKeysChanged();
    }
    emit secretsChanged();
}

} // namespace Kodometer
