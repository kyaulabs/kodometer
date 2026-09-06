#include <kodometer/kwallet_credential_store.hpp>

#include <KWallet>

namespace Kodometer {
namespace {

const QString FolderName = QStringLiteral("Kodometer");

class KWalletCredentialBackend final : public CredentialBackend
{
  public:
    using CredentialBackend::CredentialBackend;

    void open() override
    {
        if (m_wallet != nullptr) {
            m_wallet->deleteLater();
            m_wallet = nullptr;
        }
        m_wallet = KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), 0,
                                               KWallet::Wallet::Asynchronous);
        if (m_wallet == nullptr) {
            emit openFinished(false, QStringLiteral("KWallet is disabled or unavailable"));
            return;
        }
        if (m_wallet->parent() == nullptr) {
            m_wallet->setParent(this);
        }
        connect(m_wallet, &KWallet::Wallet::walletOpened, this,
                [this](bool success) { walletOpened(success); });
        connect(m_wallet, &KWallet::Wallet::walletClosed, this, &CredentialBackend::closed);
        connect(m_wallet, &KWallet::Wallet::folderUpdated, this, [this](const QString &folder) {
            if (folder == FolderName) {
                emit changed();
            }
        });
    }

    std::optional<QMap<QString, QString>> readSecrets(const QStringList &keys,
                                                      QString *error) override
    {
        if (!selectFolder(error)) {
            return std::nullopt;
        }
        QMap<QString, QString> result;
        for (const QString &key : keys) {
            if (!m_wallet->hasEntry(key)) {
                continue;
            }
            QString value;
            if (m_wallet->readPassword(key, value) != 0) {
                setError(error, QStringLiteral("KWallet could not read credential %1").arg(key));
                return std::nullopt;
            }
            result.insert(key, value);
        }
        return result;
    }

    std::optional<QMap<QString, QString>> readAccountEntries(QString *error) override
    {
        if (!selectFolder(error)) {
            return std::nullopt;
        }
        const QStringList entries = m_wallet->entryList();
        if (entries.size() > 64) {
            setError(error, QStringLiteral("KWallet account folder has too many entries"));
            return std::nullopt;
        }
        QMap<QString, QString> result;
        for (const QString &entry : entries) {
            if (!WalletAccounts::isAccountEntry(entry)) {
                continue;
            }
            QString value;
            if (m_wallet->readPassword(entry, value) != 0) {
                setError(error, QStringLiteral("KWallet could not read a named account"));
                return std::nullopt;
            }
            result.insert(entry, value);
        }
        return result;
    }

    bool writeSecret(const QString &key, const QString &value, QString *error) override
    {
        if (!selectFolder(error)) {
            return false;
        }
        if (m_wallet->writePassword(key, value) != 0) {
            setError(error, QStringLiteral("KWallet could not store the credential"));
            return false;
        }
        return true;
    }

    bool removeSecret(const QString &key, QString *error) override
    {
        if (!selectFolder(error)) {
            return false;
        }
        if (m_wallet->hasEntry(key) && m_wallet->removeEntry(key) != 0) {
            setError(error, QStringLiteral("KWallet could not remove the credential"));
            return false;
        }
        return true;
    }

  private:
    static void setError(QString *error, const QString &message)
    {
        if (error != nullptr) {
            *error = message;
        }
    }

    bool selectFolder(QString *error)
    {
        if (m_wallet == nullptr || !m_wallet->isOpen()) {
            setError(error, QStringLiteral("KWallet is not open"));
            return false;
        }
        if (!m_wallet->setFolder(FolderName)) {
            setError(error, QStringLiteral("KWallet credential folder is unavailable"));
            return false;
        }
        return true;
    }

    void walletOpened(bool success)
    {
        if (!success || m_wallet == nullptr || !m_wallet->isOpen()) {
            emit openFinished(false, QStringLiteral("KWallet access was denied"));
            return;
        }
        if (!m_wallet->hasFolder(FolderName) && !m_wallet->createFolder(FolderName)) {
            emit openFinished(false,
                              QStringLiteral("KWallet credential folder could not be created"));
            return;
        }
        QString error;
        if (!selectFolder(&error)) {
            emit openFinished(false, error);
            return;
        }
        emit openFinished(true, {});
    }

    KWallet::Wallet *m_wallet = nullptr;
};

} // namespace

KWalletCredentialStore::KWalletCredentialStore(QObject *parent)
    : CredentialStore(new KWalletCredentialBackend, parent)
{}

} // namespace Kodometer
