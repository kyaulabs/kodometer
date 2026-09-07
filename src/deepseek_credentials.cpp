#include <kodometer/deepseek_credentials.hpp>

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

std::optional<DeepSeekCredentials>
DeepSeekCredentialResolver::resolve(const QMap<QString, QString> &environment, QString *error)
{
    QString apiKey = cleaned(environment.value(QStringLiteral("DEEPSEEK_API_KEY")));
    if (apiKey.isEmpty()) {
        apiKey = cleaned(environment.value(QStringLiteral("DEEPSEEK_KEY")));
    }
    if (apiKey.isEmpty()) {
        setError(error, QStringLiteral("DeepSeek API key is missing"));
        return std::nullopt;
    }
    if (apiKey.contains(QLatin1Char('\n')) || apiKey.contains(QLatin1Char('\r'))) {
        setError(error, QStringLiteral("DeepSeek API key contains invalid characters"));
        return std::nullopt;
    }
    return DeepSeekCredentials{apiKey};
}

} // namespace Kodometer
