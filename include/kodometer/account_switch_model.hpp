#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QVariantMap>
#include <kodometer/wallet_accounts.hpp>

namespace Kodometer {

// Produces metadata-only configuration edits; never changes credentials or starts requests.
class AccountSwitchModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString oauthConfiguration MEMBER m_oauthConfiguration NOTIFY changed)
    Q_PROPERTY(QVariantMap walletSelections MEMBER m_walletSelections NOTIFY changed)
    Q_PROPERTY(QStringList disabledProviders MEMBER m_disabledProviders NOTIFY changed)
    Q_PROPERTY(Kodometer::WalletAccounts *accounts READ accounts WRITE setAccounts NOTIFY changed)
    Q_PROPERTY(QVariantList providers READ providers NOTIFY changed)

  public:
    using QObject::QObject;
    [[nodiscard]] WalletAccounts *accounts() const;
    void setAccounts(WalletAccounts *accounts);
    [[nodiscard]] QVariantList providers() const;
    Q_INVOKABLE QVariantMap selectionChange(const QString &provider, const QString &id) const;

  signals:
    void changed();

  private:
    QString m_oauthConfiguration = QStringLiteral("{}");
    QVariantMap m_walletSelections;
    QStringList m_disabledProviders;
    QPointer<WalletAccounts> m_accounts;
};

} // namespace Kodometer
