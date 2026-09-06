#pragma once

#include <kodometer/wallet_accounts.hpp>

#include <QMap>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>

#include <optional>

namespace Kodometer {

class CredentialBackend : public QObject
{
    Q_OBJECT

  public:
    using QObject::QObject;
    ~CredentialBackend() override = default;

    virtual void open() = 0;
    [[nodiscard]] virtual std::optional<QMap<QString, QString>>
    readSecrets(const QStringList &keys, QString *error = nullptr) = 0;
    [[nodiscard]] virtual std::optional<QMap<QString, QString>> readAccountEntries(QString *)
    {
        return std::nullopt; // Legacy-only backends cannot offer named accounts.
    }
    virtual bool writeSecret(const QString &key, const QString &value,
                             QString *error = nullptr) = 0;
    virtual bool removeSecret(const QString &key, QString *error = nullptr) = 0;

  signals:
    void openFinished(bool success, const QString &error);
    void changed();
    void closed();
};

class CredentialStore : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(Kodometer::WalletAccounts *accounts READ accounts CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QStringList configuredKeys READ configuredKeys NOTIFY configuredKeysChanged)

  public:
    static constexpr qsizetype MaximumSecretSize = 64 * 1024;

    explicit CredentialStore(CredentialBackend *backend, QObject *parent = nullptr);

    [[nodiscard]] WalletAccounts *accounts() noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QStringList configuredKeys() const;
    [[nodiscard]] QMap<QString, QString> secrets() const;
    [[nodiscard]] static QStringList supportedKeys();

    Q_INVOKABLE void open();
    Q_INVOKABLE bool hasSecret(const QString &key) const;
    Q_INVOKABLE bool saveSecret(const QString &key, const QString &value);
    Q_INVOKABLE bool removeSecret(const QString &key);

  signals:
    void readyChanged();
    void busyChanged();
    void errorChanged();
    void configuredKeysChanged();
    void secretsChanged();

  private:
    [[nodiscard]] static bool supportedKey(const QString &key);
    [[nodiscard]] bool reload();
    void handleOpenFinished(bool success, const QString &error);
    void handleClosed();
    void setReady(bool ready);
    void setBusy(bool busy);
    void setError(const QString &error);
    void setSecrets(const QMap<QString, QString> &secrets);

    CredentialBackend *m_backend = nullptr;
    WalletAccounts m_accounts;
    bool m_backendOpen = false;
    QMap<QString, QString> m_secrets;
    QString m_error;
    bool m_ready = false;
    bool m_busy = false;
};

} // namespace Kodometer
