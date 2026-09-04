#include <kodometer/codex_credentials.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QTimeZone>

#include <cmath>
#include <limits>

#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif

namespace Kodometer {
namespace {

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

QString nonEmpty(const QJsonValue &value)
{
    return value.isString() ? value.toString().trimmed() : QString{};
}

QString tokenValue(const QJsonObject &tokens, const QString &snakeCase, const QString &camelCase)
{
    const QString snakeValue = nonEmpty(tokens.value(snakeCase));
    return snakeValue.isEmpty() ? nonEmpty(tokens.value(camelCase)) : snakeValue;
}

QJsonObject jwtClaims(const QString &token)
{
    const QStringList parts = token.split(QLatin1Char('.'), Qt::KeepEmptyParts);
    if (parts.size() != 3 || parts.at(1).isEmpty()) {
        return {};
    }
    const QByteArray payload =
        QByteArray::fromBase64(parts.at(1).toLatin1(), QByteArray::Base64UrlEncoding);
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object();
}

QString accountFromClaims(const QJsonObject &claims)
{
    const QString direct = nonEmpty(claims.value(QStringLiteral("chatgpt_account_id")));
    if (!direct.isEmpty()) {
        return direct;
    }
    const QJsonObject auth = claims.value(QStringLiteral("https://api.openai.com/auth")).toObject();
    const QString nested = nonEmpty(auth.value(QStringLiteral("chatgpt_account_id")));
    if (!nested.isEmpty()) {
        return nested;
    }
    const QJsonArray organizations = claims.value(QStringLiteral("organizations")).toArray();
    for (const QJsonValue &organization : organizations) {
        const QString id = nonEmpty(organization.toObject().value(QStringLiteral("id")));
        if (!id.isEmpty()) {
            return id;
        }
    }
    return {};
}

QDateTime expirationFromClaims(const QJsonObject &claims)
{
    const QJsonValue expiration = claims.value(QStringLiteral("exp"));
    if (!expiration.isDouble()) {
        return {};
    }
    const double seconds = expiration.toDouble();
    if (!std::isfinite(seconds) || std::floor(seconds) != seconds ||
        seconds < static_cast<double>(std::numeric_limits<qint64>::min()) ||
        seconds > static_cast<double>(std::numeric_limits<qint64>::max())) {
        return {};
    }
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(seconds), QTimeZone::UTC);
}

QDateTime parseTimestamp(const QJsonValue &value)
{
    if (!value.isString()) {
        return {};
    }
    return QDateTime::fromString(value.toString(), Qt::ISODate);
}

bool permissionsExposeCredentials(QFileDevice::Permissions permissions)
{
    constexpr auto exposed = QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                             QFileDevice::ReadOther | QFileDevice::WriteOther;
    return (permissions & exposed) != QFileDevice::Permissions{};
}

bool validateExistingFile(const QFileInfo &info, QString *error)
{
    if (info.isSymbolicLink()) {
        setError(error, QStringLiteral("Codex auth file must not be a symbolic link"));
        return false;
    }
    if (!info.exists()) {
        setError(error, QStringLiteral("Codex auth file was not found"));
        return false;
    }
    if (!info.isFile()) {
        setError(error, QStringLiteral("Codex auth path is not a regular file"));
        return false;
    }
#if defined(Q_OS_UNIX)
    if (info.ownerId() != static_cast<uint>(geteuid())) {
        setError(error, QStringLiteral("Codex auth file is owned by another user"));
        return false;
    }
#endif
    if (permissionsExposeCredentials(info.permissions())) {
        setError(error, QStringLiteral("Codex auth file permissions expose credentials"));
        return false;
    }
    if (info.size() > CodexCredentialStore::MaximumFileSize) {
        setError(error, QStringLiteral("Codex auth file exceeds the 1 MiB limit"));
        return false;
    }
    return true;
}

} // namespace

bool CodexCredentials::needsRefresh(const QDateTime &now) const
{
    if (apiKey) {
        return false;
    }
    if (expiresAt.isValid()) {
        return now.secsTo(expiresAt) <= 5 * 60;
    }
    if (!lastRefresh.isValid()) {
        return true;
    }
    return lastRefresh.secsTo(now) > 8 * 24 * 60 * 60;
}

QString CodexCredentials::redactedEmail() const
{
    if (email.isEmpty()) {
        return {};
    }
    const qsizetype separator = email.indexOf(QLatin1Char('@'));
    if (separator < 0) {
        return QStringLiteral("redacted");
    }
    return QStringLiteral("redacted") + email.mid(separator);
}

QString CodexCredentialStore::authenticationFilePath(const QMap<QString, QString> &environment,
                                                     const QString &homeDirectory)
{
    QString root = environment.value(QStringLiteral("CODEX_HOME")).trimmed();
    if (root.isEmpty()) {
        root = homeDirectory.isEmpty() ? QDir::homePath() : homeDirectory;
        root = QDir(root).filePath(QStringLiteral(".codex"));
    }
    return QDir::cleanPath(QDir(root).filePath(QStringLiteral("auth.json")));
}

std::optional<CodexCredentials> CodexCredentialStore::load(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (!validateExistingFile(info, error)) {
        return std::nullopt;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Codex auth file could not be read"));
        return std::nullopt;
    }
    const QByteArray data = file.read(MaximumFileSize + 1);
    if (data.size() > MaximumFileSize) {
        setError(error, QStringLiteral("Codex auth file exceeds the 1 MiB limit"));
        return std::nullopt;
    }
    return parse(data, error);
}

std::optional<CodexCredentials> CodexCredentialStore::parse(const QByteArray &data, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Codex auth file contains invalid JSON"));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("Codex auth file must contain a JSON object"));
        return std::nullopt;
    }

    const QJsonObject root = document.object();
    const QString apiKey = nonEmpty(root.value(QStringLiteral("OPENAI_API_KEY")));
    if (!apiKey.isEmpty()) {
        CodexCredentials credentials;
        credentials.accessToken = apiKey;
        credentials.apiKey = true;
        credentials.document = root;
        return credentials;
    }

    const QJsonObject tokens = root.value(QStringLiteral("tokens")).toObject();
    const QString accessToken =
        tokenValue(tokens, QStringLiteral("access_token"), QStringLiteral("accessToken"));
    const QString refreshToken =
        tokenValue(tokens, QStringLiteral("refresh_token"), QStringLiteral("refreshToken"));
    if (accessToken.isEmpty() || refreshToken.isEmpty()) {
        setError(error, QStringLiteral("Codex auth file contains no usable credentials"));
        return std::nullopt;
    }

    CodexCredentials credentials;
    credentials.accessToken = accessToken;
    credentials.refreshToken = refreshToken;
    credentials.idToken = tokenValue(tokens, QStringLiteral("id_token"), QStringLiteral("idToken"));
    credentials.accountId =
        tokenValue(tokens, QStringLiteral("account_id"), QStringLiteral("accountId"));
    credentials.lastRefresh = parseTimestamp(root.value(QStringLiteral("last_refresh")));
    credentials.document = root;

    const QJsonObject idClaims = jwtClaims(credentials.idToken);
    const QJsonObject accessClaims = jwtClaims(credentials.accessToken);
    if (credentials.accountId.isEmpty()) {
        credentials.accountId = accountFromClaims(idClaims);
        if (credentials.accountId.isEmpty()) {
            credentials.accountId = accountFromClaims(accessClaims);
        }
    }
    credentials.email = nonEmpty(idClaims.value(QStringLiteral("email")));
    if (credentials.email.isEmpty()) {
        credentials.email = nonEmpty(accessClaims.value(QStringLiteral("email")));
    }
    credentials.expiresAt = expirationFromClaims(accessClaims);
    return credentials;
}

bool CodexCredentialStore::save(const QString &path, const CodexCredentials &credentials,
                                QString *error)
{
    const QFileInfo info(path);
    if (!validateExistingFile(info, error)) {
        return false;
    }
    if (credentials.apiKey) {
        setError(error, QStringLiteral("Codex API-key credentials cannot be rotated"));
        return false;
    }

    QJsonObject root = credentials.document;
    QJsonObject tokens = root.value(QStringLiteral("tokens")).toObject();
    tokens.insert(QStringLiteral("access_token"), credentials.accessToken);
    tokens.insert(QStringLiteral("refresh_token"), credentials.refreshToken);
    if (!credentials.idToken.isEmpty()) {
        tokens.insert(QStringLiteral("id_token"), credentials.idToken);
    }
    if (!credentials.accountId.isEmpty()) {
        tokens.insert(QStringLiteral("account_id"), credentials.accountId);
    }
    root.insert(QStringLiteral("tokens"), tokens);
    if (credentials.lastRefresh.isValid()) {
        root.insert(QStringLiteral("last_refresh"),
                    credentials.lastRefresh.toUTC().toString(Qt::ISODateWithMs));
    }

    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Codex auth file could not be opened for rotation"));
        return false;
    }
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        file.cancelWriting();
        setError(error, QStringLiteral("Codex auth file permissions could not be secured"));
        return false;
    }
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        setError(error, QStringLiteral("Codex auth file rotation could not be committed"));
        return false;
    }
    return true;
}

} // namespace Kodometer
