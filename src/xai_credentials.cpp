#include <kodometer/xai_credentials.hpp>

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

} // namespace

std::optional<XaiCredentials>
XaiCredentialResolver::resolve(const QMap<QString, QString> &environment, QString *error)
{
    const QString apiKey = cleaned(environment.value(QStringLiteral("XAI_MANAGEMENT_API_KEY")));
    if (apiKey.isEmpty()) {
        setError(error, QStringLiteral("xAI Management API key is missing"));
        return std::nullopt;
    }
    const QString teamId = cleaned(environment.value(QStringLiteral("XAI_TEAM_ID")));
    if (teamId.isEmpty()) {
        setError(error, QStringLiteral("xAI team ID is missing"));
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_START -- tested QString comparisons contain Qt branches
    if (teamId == QStringLiteral(".") || teamId == QStringLiteral("..") ||
        teamId.contains(QLatin1Char('/'))) {
        setError(error,
                 QStringLiteral("xAI team ID must be a single identifier without path separators"));
        return std::nullopt;
    }
    const XaiCredentials credentials{apiKey, teamId};
    // GCOVR_EXCL_BR_STOP
    return credentials;
}

} // namespace Kodometer
