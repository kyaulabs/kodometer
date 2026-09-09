#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <kodometer/provider_adapter.hpp>

namespace Kodometer {

QList<QByteArray> ProviderAdapter::historyIdentities() const
{
    return m_historyIdentities;
}

QByteArray ProviderAdapter::defaultCredentialContext() const
{
    return {};
}

QByteArray ProviderAdapter::credentialContext(const QMap<QString, QString> &environment,
                                              std::initializer_list<QStringView> keys) const
{
    const auto effective = environmentWithCredentialOverrides(environment);
    QJsonObject selected;
    for (const QStringView keyView : keys) {
        const QString key = keyView.toString();
        selected.insert(key, effective.value(key));
    }
    const QJsonDocument document(selected);
    const QByteArray bytes = document.toJson(QJsonDocument::Compact);
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

void ProviderAdapter::setHistoryIdentity(const QByteArray &identity, bool rotation)
{
    if (identity.isEmpty()) {
        m_historyIdentities.clear();
        return;
    }
    const auto digest = QCryptographicHash::hash(identity, QCryptographicHash::Sha256);
    if (!rotation && (m_historyIdentities.isEmpty() || m_historyIdentities.last() != digest))
        m_historyIdentities.clear();
    m_historyIdentities.removeAll(digest);
    m_historyIdentities.append(digest);
    if (m_historyIdentities.size() > 3)
        m_historyIdentities.removeFirst();
}

void ProviderAdapter::setAccountCredential(const std::optional<QString> &credential,
                                           const QString &managementCredential,
                                           const QString &teamId, const QVariantMap &zaiOptions)
{
    m_accountCredential = credential;
    m_accountManagementCredential = credential ? managementCredential : QString{};
    m_accountTeamId = credential ? teamId : QString{};
    m_accountZaiOptions = credential ? zaiOptions : QVariantMap{};
}

QVariantMap ProviderAdapter::selectedAccountZaiOptions() const
{
    return m_accountZaiOptions;
}

QString ProviderAdapter::selectedAccountTeamId() const
{
    return m_accountTeamId;
}

QString ProviderAdapter::selectedAccountManagementCredential() const
{
    return m_accountManagementCredential;
}

std::optional<QString> ProviderAdapter::selectedAccountCredential() const
{
    return m_accountCredential;
}

void ProviderAdapter::setCredentialOverrides(const QMap<QString, QString> &overrides)
{
    m_credentialOverrides = overrides;
}

QMap<QString, QString>
ProviderAdapter::environmentWithCredentialOverrides(const QMap<QString, QString> &environment) const
{
    QMap<QString, QString> result = environment;
    for (auto iterator = m_credentialOverrides.cbegin(); iterator != m_credentialOverrides.cend();
         ++iterator) {
        if (result.value(iterator.key()).trimmed().isEmpty()) {
            result.insert(iterator.key(), iterator.value());
        }
    }
    return result;
}

} // namespace Kodometer
