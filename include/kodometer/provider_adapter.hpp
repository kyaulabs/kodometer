#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <optional>

namespace Kodometer {

class ProviderAdapter : public QObject
{
    Q_OBJECT

  public:
    using QObject::QObject;
    ~ProviderAdapter() override = default;

    [[nodiscard]] virtual QString providerId() const = 0;
    virtual void refresh() = 0;
    // Only adapters with provider-owned credential folders override this hook.
    virtual void setProfileDirectory(const QString &) {}

    void setCredentialOverrides(const QMap<QString, QString> &overrides);
    void setAccountCredential(const std::optional<QString> &credential,
                              const QString &managementCredential = {});

  signals:
    void refreshSucceeded(const QVariantMap &provider);
    void refreshFailed(const QString &error);

  protected:
    [[nodiscard]] std::optional<QString> selectedAccountCredential() const;
    [[nodiscard]] QString selectedAccountManagementCredential() const;
    [[nodiscard]] QMap<QString, QString>
    environmentWithCredentialOverrides(const QMap<QString, QString> &environment) const;

  private:
    QMap<QString, QString> m_credentialOverrides;
    std::optional<QString> m_accountCredential;
    QString m_accountManagementCredential;
};

} // namespace Kodometer
