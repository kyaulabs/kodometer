#include <kodometer/openrouter_credentials.hpp>

namespace Kodometer {
namespace {

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

QString cleaned(QString value)
{
    value = value.trimmed();
    // GCOVR_EXCL_BR_START -- tested QString and QChar accessors contain Qt branches
    if (value.size() >= 2) {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('\'') && last == QLatin1Char('\'')) ||
            (first == QLatin1Char('"') && last == QLatin1Char('"'))) {
            value = value.mid(1, value.size() - 2).trimmed();
        }
    }
    // GCOVR_EXCL_BR_STOP
    return value;
}

bool invalidHeaderValue(const QString &value)
{
    return value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r'));
}

} // namespace

std::optional<OpenRouterCredentials>
OpenRouterCredentialResolver::resolve(const QMap<QString, QString> &environment, QString *error)
{
    OpenRouterCredentials credentials;
    credentials.apiKey = cleaned(environment.value(QStringLiteral("OPENROUTER_API_KEY")));
    credentials.managementApiKey =
        cleaned(environment.value(QStringLiteral("OPENROUTER_MANAGEMENT_API_KEY")));
    credentials.httpReferer = cleaned(environment.value(QStringLiteral("OPENROUTER_HTTP_REFERER")));
    credentials.clientTitle = cleaned(environment.value(QStringLiteral("OPENROUTER_X_TITLE")));
    if (credentials.clientTitle.isEmpty()) {
        credentials.clientTitle = QStringLiteral("Kodometer");
    }

    if (credentials.apiKey.isEmpty()) {
        setError(error, QStringLiteral("OpenRouter API key is missing"));
        return std::nullopt;
    }
    if (invalidHeaderValue(credentials.apiKey)) {
        setError(error, QStringLiteral("OpenRouter API key contains invalid characters"));
        return std::nullopt;
    }
    if (invalidHeaderValue(credentials.managementApiKey)) {
        setError(error,
                 QStringLiteral("OpenRouter Management API key contains invalid characters"));
        return std::nullopt;
    }
    if (invalidHeaderValue(credentials.httpReferer)) {
        setError(error, QStringLiteral("OpenRouter HTTP referer contains invalid characters"));
        return std::nullopt;
    }
    if (invalidHeaderValue(credentials.clientTitle)) {
        setError(error, QStringLiteral("OpenRouter client title contains invalid characters"));
        return std::nullopt;
    }
    return credentials;
}

} // namespace Kodometer
