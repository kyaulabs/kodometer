#include <kodometer/gemini_credentials.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>
#include <QTimeZone>

#include <cmath>

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
    return value.isString() ? value.toString().trimmed() : QString{}; // GCOVR_EXCL_BR_LINE
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
        setError(error, QStringLiteral("Gemini credentials must not be a symbolic link"));
        return false;
    }
    if (!info.exists()) {
        setError(error, QStringLiteral("Gemini credentials were not found"));
        return false;
    }
    if (!info.isFile()) {
        setError(error, QStringLiteral("Gemini credential path is not a regular file"));
        return false;
    }
#if defined(Q_OS_UNIX)
    if (info.ownerId() != static_cast<uint>(geteuid())) {
        setError(error, QStringLiteral("Gemini credentials are owned by another user"));
        return false;
    }
#endif
    if (permissionsExposeCredentials(info.permissions())) {
        setError(error, QStringLiteral("Gemini credential permissions expose secrets"));
        return false;
    }
    if (info.size() > GeminiCredentialStore::MaximumFileSize) {
        setError(error, QStringLiteral("Gemini credentials exceed the 1 MiB limit"));
        return false;
    }
    return true;
}

std::optional<QDateTime> expiration(const QJsonValue &value)
{
    if (value.isNull() || value.isUndefined()) {
        return QDateTime{};
    }
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double milliseconds = value.toDouble();
    constexpr double MaximumMilliseconds = 8'210'266'876'799'000.0;
    if (!std::isfinite(milliseconds) || std::floor(milliseconds) != milliseconds ||
        milliseconds < 0.0 || milliseconds > MaximumMilliseconds) { // GCOVR_EXCL_BR_LINE
        return std::nullopt;
    }
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(milliseconds), QTimeZone::UTC);
}

void extractClaims(GeminiCredentials &credentials)
{
    const QStringList segments = credentials.idToken.split(QLatin1Char('.'));
    if (segments.size() < 2) {
        return;
    }
    const QByteArray decoded = QByteArray::fromBase64(segments.at(1).toLatin1(),
                                                      QByteArray::Base64UrlEncoding |
                                                          QByteArray::AbortOnBase64DecodingErrors);
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(decoded, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }
    credentials.email = nonEmpty(document.object().value(QStringLiteral("email")));
    credentials.hostedDomain = nonEmpty(document.object().value(QStringLiteral("hd")));
}

std::optional<GeminiOAuthClient> parseOAuthJavaScript(const QByteArray &data)
{
    const QString source = QString::fromUtf8(data);
    const QRegularExpression clientExpression(
        QStringLiteral(R"((?:const|let|var)?\s*OAUTH_CLIENT_ID\s*=\s*['"]([^'"]+)['"]\s*;)"));
    const QRegularExpression secretExpression(
        QStringLiteral(R"((?:const|let|var)?\s*OAUTH_CLIENT_SECRET\s*=\s*['"]([^'"]+)['"]\s*;)"));
    const QRegularExpressionMatch clientMatch = clientExpression.match(source);
    const QRegularExpressionMatch secretMatch = secretExpression.match(source);
    if (!clientMatch.hasMatch() || !secretMatch.hasMatch()) {
        return std::nullopt;
    }
    const QString clientId = clientMatch.captured(1).trimmed();
    const QString clientSecret = secretMatch.captured(1).trimmed();
    if (clientId.isEmpty() || clientSecret.isEmpty()) { // GCOVR_EXCL_BR_LINE
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_START -- QString value construction branches
    const GeminiOAuthClient client{clientId, clientSecret};
    // GCOVR_EXCL_BR_STOP
    return client;
}

std::optional<GeminiOAuthClient> loadOAuthJavaScript(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray data = file.read(GeminiCredentialStore::MaximumFileSize + 1);
    if (data.size() > GeminiCredentialStore::MaximumFileSize) {
        return std::nullopt;
    }
    return parseOAuthJavaScript(data);
}

QStringList discoveredOAuthPaths(const QMap<QString, QString> &environment)
{
    constexpr auto OAuthRelativePath = "dist/src/code_assist/oauth2.js";
    constexpr auto CoreRelativePath = "@google/gemini-cli-core/dist/src/code_assist/oauth2.js";
    QStringList paths;
    const QString pathEnvironment = environment.value(QStringLiteral("PATH"));
    // GCOVR_EXCL_BR_START -- tested filesystem discovery includes Qt iterator branches
    for (const QString &directory :
         pathEnvironment.split(QDir::listSeparator(), Qt::SkipEmptyParts)) {
        QFileInfo executable(QDir(directory).filePath(QStringLiteral("gemini")));
        if (!executable.exists() || !executable.isExecutable()) {
            continue;
        }
        QString current = executable.canonicalFilePath();
        if (current.isEmpty()) {
            current = executable.absoluteFilePath();
        }
        if (QFileInfo(current).isFile()) {
            current = QFileInfo(current).absolutePath();
        }
        for (int ascent = 0; ascent <= 8; ++ascent) {
            const QDir root(current);
            paths.append(root.filePath(QString::fromLatin1(OAuthRelativePath)));
            paths.append(root.filePath(QString::fromLatin1(CoreRelativePath)));
            paths.append(root.filePath(QStringLiteral("node_modules/") +
                                       QString::fromLatin1(CoreRelativePath)));
            const QString parent = QFileInfo(current).absolutePath();
            if (parent == current) {
                break;
            }
            current = parent;
        }
    }

    QString home = environment.value(QStringLiteral("HOME")).trimmed();
    if (home.isEmpty()) {
        home = QDir::homePath();
    }
    for (const QString &root :
         {QStringLiteral("/usr/lib/node_modules"), QStringLiteral("/usr/local/lib/node_modules"),
          QDir(home).filePath(QStringLiteral(".npm-global/lib/node_modules"))}) {
        paths.append(QDir(root).filePath(QString::fromLatin1(CoreRelativePath)));
        paths.append(QDir(root).filePath(QStringLiteral("@google/gemini-cli/node_modules/") +
                                         QString::fromLatin1(CoreRelativePath)));
    }
    // GCOVR_EXCL_BR_STOP
    return paths;
}

} // namespace

bool GeminiCredentials::needsRefresh(const QDateTime &now) const
{
    // GCOVR_EXCL_BR_START -- tested QString and QDateTime accessors contain Qt branches
    const bool refresh =
        accessToken.isEmpty() || !expiresAt.isValid() || now.secsTo(expiresAt) <= 5 * 60;
    // GCOVR_EXCL_BR_STOP
    return refresh;
}

QString GeminiCredentials::redactedEmail() const
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

QString GeminiCredentialStore::authenticationFilePath(const QString &homeDirectory)
{
    const QString home =
        homeDirectory.isEmpty() ? QDir::homePath() : homeDirectory; // GCOVR_EXCL_BR_LINE
    return QDir::cleanPath(QDir(home).filePath(QStringLiteral(".gemini/oauth_creds.json")));
}

QString GeminiCredentialStore::settingsFilePath(const QString &homeDirectory)
{
    const QString home =
        homeDirectory.isEmpty() ? QDir::homePath() : homeDirectory; // GCOVR_EXCL_BR_LINE
    return QDir::cleanPath(QDir(home).filePath(QStringLiteral(".gemini/settings.json")));
}

GeminiAuthType GeminiCredentialStore::selectedAuthentication(const QString &settingsPath)
{
    QFile file(settingsPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return GeminiAuthType::Unknown;
    }
    const QByteArray data = file.read(MaximumFileSize + 1);
    if (data.size() > MaximumFileSize) {
        return GeminiAuthType::Unknown;
    }
    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (!document.isObject()) {
        return GeminiAuthType::Unknown;
    }
    const QString selected = document.object()
                                 .value(QStringLiteral("security"))
                                 .toObject()
                                 .value(QStringLiteral("auth"))
                                 .toObject()
                                 .value(QStringLiteral("selectedType"))
                                 .toString()
                                 .trimmed();
    if (selected == QStringLiteral("oauth-personal")) {
        return GeminiAuthType::OAuthPersonal;
    }
    if (selected == QStringLiteral("api-key") || selected == QStringLiteral("gemini-api-key")) {
        return GeminiAuthType::ApiKey;
    }
    if (selected == QStringLiteral("vertex-ai")) {
        return GeminiAuthType::VertexAi;
    }
    return GeminiAuthType::Unknown;
}

std::optional<GeminiCredentials> GeminiCredentialStore::load(const QString &path, QString *error)
{
    if (!validateExistingFile(QFileInfo(path), error)) {
        return std::nullopt;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { // GCOVR_EXCL_LINE -- validated file changed concurrently
        setError(error, QStringLiteral("Gemini credentials could not be read")); // GCOVR_EXCL_LINE
        return std::nullopt;                                                     // GCOVR_EXCL_LINE
    }
    const QByteArray data = file.read(MaximumFileSize + 1);
    if (data.size() > MaximumFileSize) { // GCOVR_EXCL_LINE -- file grew after validation
        setError(error,
                 QStringLiteral("Gemini credentials exceed the 1 MiB limit")); // GCOVR_EXCL_LINE
        return std::nullopt;                                                   // GCOVR_EXCL_LINE
    }
    return parse(data, error);
}

std::optional<GeminiCredentials> GeminiCredentialStore::parse(const QByteArray &data,
                                                              QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Gemini credentials contain invalid JSON"));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("Gemini credentials must contain a JSON object"));
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    const QString accessToken = nonEmpty(root.value(QStringLiteral("access_token")));
    const QString refreshToken = nonEmpty(root.value(QStringLiteral("refresh_token")));
    if (accessToken.isEmpty() && refreshToken.isEmpty()) {
        setError(error, QStringLiteral("Gemini OAuth tokens are missing"));
        return std::nullopt;
    }
    const auto expiresAt = expiration(root.value(QStringLiteral("expiry_date")));
    if (!expiresAt) {
        setError(error, QStringLiteral("Gemini OAuth expiry is invalid"));
        return std::nullopt;
    }

    GeminiCredentials credentials;
    credentials.accessToken = accessToken;
    credentials.refreshToken = refreshToken;
    credentials.idToken = nonEmpty(root.value(QStringLiteral("id_token")));
    credentials.expiresAt = *expiresAt;
    credentials.document = root;
    extractClaims(credentials);
    return credentials;
}

bool GeminiCredentialStore::save(const QString &path, const GeminiCredentials &credentials,
                                 QString *error)
{
    if (!validateExistingFile(QFileInfo(path), error)) {
        return false;
    }
    QJsonObject root = credentials.document;
    root.insert(QStringLiteral("access_token"), credentials.accessToken);
    if (credentials.refreshToken.isEmpty()) {
        root.remove(QStringLiteral("refresh_token"));
    }
    else {
        root.insert(QStringLiteral("refresh_token"), credentials.refreshToken);
    }
    if (credentials.idToken.isEmpty()) {
        root.remove(QStringLiteral("id_token"));
    }
    else {
        root.insert(QStringLiteral("id_token"), credentials.idToken);
    }
    if (credentials.expiresAt.isValid()) {
        root.insert(QStringLiteral("expiry_date"), credentials.expiresAt.toMSecsSinceEpoch());
    }
    else {
        root.remove(QStringLiteral("expiry_date"));
    }

    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    // GCOVR_EXCL_START -- failures require concurrent path mutation or storage faults
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Gemini credentials could not be opened for rotation"));
        return false;
    }
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        file.cancelWriting();
        setError(error, QStringLiteral("Gemini credential permissions could not be secured"));
        return false;
    }
    const QByteArray output = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(output) != output.size() || !file.commit()) {
        setError(error, QStringLiteral("Gemini credential rotation could not be committed"));
        return false;
    }
    // GCOVR_EXCL_STOP
    return true;
}

GeminiOAuthClient GeminiOAuthConfig::resolve(const QMap<QString, QString> &environment)
{
    const QString clientId = environment.value(QStringLiteral("GEMINI_OAUTH_CLIENT_ID")).trimmed();
    const QString clientSecret =
        environment.value(QStringLiteral("GEMINI_OAUTH_CLIENT_SECRET")).trimmed();
    if (!clientId.isEmpty() && !clientSecret.isEmpty()) {
        return {clientId, clientSecret};
    }

    const QString scriptPath = environment.value(QStringLiteral("GEMINI_OAUTH2_JS_PATH")).trimmed();
    if (!scriptPath.isEmpty()) {
        const auto parsed = loadOAuthJavaScript(scriptPath);
        if (parsed) {
            return *parsed;
        }
    }

    for (const QString &path : discoveredOAuthPaths(environment)) {
        const auto parsed = loadOAuthJavaScript(path);
        if (parsed) {
            return *parsed;
        }
    }
    return {};
}

} // namespace Kodometer
