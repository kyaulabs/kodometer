#include <kodometer/provider_adapter.hpp>

namespace Kodometer {

void ProviderAdapter::setAccountCredential(const std::optional<QString> &credential)
{
    m_accountCredential = credential;
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
