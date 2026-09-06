#pragma once

#include <QMap>
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

#include <optional>

namespace Kodometer {
class CredentialBackend;

class WalletAccounts : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantMap providers READ providers NOTIFY changed)

  public:
    explicit WalletAccounts(CredentialBackend *backend, QObject *parent = nullptr);
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QVariantMap providers() const;
    [[nodiscard]] QVariantList entries(const QString &provider) const;
    [[nodiscard]] QString name(const QString &provider, const QString &id) const;
    // C++ only: no secret-reading QML API.
    [[nodiscard]] std::optional<QString> key(const QString &provider, const QString &id) const;
    [[nodiscard]] QString managementKey(const QString &provider, const QString &id) const;
    [[nodiscard]] QString teamId(const QString &provider, const QString &id) const;
    [[nodiscard]] static bool supportsProvider(const QString &provider);
    [[nodiscard]] static bool isAccountEntry(const QString &entry);

    void setAvailable(bool available);
    bool reload();
    Q_INVOKABLE QString addAccount(const QString &provider, const QString &name, const QString &key,
                                   const QString &managementKey = {}, const QString &teamId = {});
    Q_INVOKABLE bool replaceAccount(const QString &provider, const QString &id, const QString &key,
                                    const QString &managementKey = {}, const QString &teamId = {});
    Q_INVOKABLE bool removeAccount(const QString &provider, const QString &id);

  signals:
    void changed();

  private:
    struct Account
    {
        QString name;
        QString key;
        QString managementKey;
        QString teamId;
        bool operator==(const Account &) const = default;
    };
    [[nodiscard]] static std::optional<Account> validated(const QString &provider,
                                                          const QString &name, const QString &key,
                                                          const QString &managementKey,
                                                          const QString &teamId);
    [[nodiscard]] static QString entryName(const QString &provider, const QString &id);
    bool editable();
    bool write(const QString &entry, const Account &account);
    void setError(const QString &error);
    void unavailable(const QString &error);

    CredentialBackend *m_backend;
    QMap<QString, Account> m_entries;
    QString m_error;
    quint64 m_readRevision = 0;
    bool m_available = false;
    bool m_ready = false;
};
} // namespace Kodometer
