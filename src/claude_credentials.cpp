#include <kodometer/claude_credentials.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
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

QString resolveDirectory(const QString &path, const QString &workingDirectory)
{
    if (QDir::isAbsolutePath(path)) {
        return QDir::cleanPath(path);
    }
    const QString base =
        workingDirectory.isEmpty() ? QDir::currentPath() : workingDirectory; // GCOVR_EXCL_BR_LINE
    return QDir::cleanPath(QDir(base).absoluteFilePath(path));
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
        setError(error, QStringLiteral("Claude credentials must not be a symbolic link"));
        return false;
    }
    if (!info.exists()) {
        setError(error, QStringLiteral("Claude credentials were not found"));
        return false;
    }
    if (!info.isFile()) {
        setError(error, QStringLiteral("Claude credential path is not a regular file"));
        return false;
    }
#if defined(Q_OS_UNIX)
    if (info.ownerId() != static_cast<uint>(geteuid())) {
        setError(error, QStringLiteral("Claude credentials are owned by another user"));
        return false;
    }
#endif
    if (permissionsExposeCredentials(info.permissions())) {
        setError(error, QStringLiteral("Claude credential permissions expose secrets"));
        return false;
    }
    if (info.size() > ClaudeCredentialStore::MaximumFileSize) {
        setError(error, QStringLiteral("Claude credentials exceed the 1 MiB limit"));
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
    constexpr double maximumMilliseconds = 8'210'266'876'799'000.0;
    if (!std::isfinite(milliseconds) ||
        std::floor(milliseconds) != milliseconds || // GCOVR_EXCL_BR_LINE
        milliseconds < 0.0 || milliseconds > maximumMilliseconds) {
        return std::nullopt;
    }
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(milliseconds), QTimeZone::UTC);
}

} // namespace

bool ClaudeCredentials::needsRefresh(const QDateTime &now) const
{
    return !expiresAt.isValid() || now.secsTo(expiresAt) <= 5 * 60;
}

QString ClaudeCredentialStore::authenticationFilePath(const QMap<QString, QString> &environment,
                                                      const QString &homeDirectory,
                                                      const QString &workingDirectory)
{
    const QString home =
        homeDirectory.isEmpty() ? QDir::homePath() : homeDirectory; // GCOVR_EXCL_BR_LINE
    QString configRoot;
    const QString configured = environment.value(QStringLiteral("CLAUDE_CONFIG_DIR"));
    if (configured.isEmpty()) {
        configRoot = QDir(home).filePath(QStringLiteral(".claude"));
    }
    else {
        configRoot = resolveDirectory(configured, workingDirectory);
    }

    QString credentialRoot = configRoot;
    if (environment.contains(QStringLiteral("CLAUDE_SECURESTORAGE_CONFIG_DIR"))) {
        const QString secureRoot =
            environment.value(QStringLiteral("CLAUDE_SECURESTORAGE_CONFIG_DIR"));
        if (!secureRoot.isEmpty()) {
            credentialRoot = resolveDirectory(secureRoot, workingDirectory);
        }
    }
    return QDir::cleanPath(QDir(credentialRoot).filePath(QStringLiteral(".credentials.json")));
}

std::optional<ClaudeCredentials> ClaudeCredentialStore::load(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (!validateExistingFile(info, error)) {
        return std::nullopt;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { // GCOVR_EXCL_LINE -- validated file changed concurrently
        setError(error, QStringLiteral("Claude credentials could not be read")); // GCOVR_EXCL_LINE
        return std::nullopt;                                                     // GCOVR_EXCL_LINE
    }
    const QByteArray data = file.read(MaximumFileSize + 1);
    if (data.size() > MaximumFileSize) { // GCOVR_EXCL_LINE -- file grew after validation
        setError(error,
                 QStringLiteral("Claude credentials exceed the 1 MiB limit")); // GCOVR_EXCL_LINE
        return std::nullopt;                                                   // GCOVR_EXCL_LINE
    }
    return parse(data, error);
}

std::optional<ClaudeCredentials> ClaudeCredentialStore::parse(const QByteArray &data,
                                                              QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Claude credentials contain invalid JSON"));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("Claude credentials must contain a JSON object"));
        return std::nullopt;
    }

    const QJsonObject root = document.object();
    const QJsonValue oauthValue = root.value(QStringLiteral("claudeAiOauth"));
    if (!oauthValue.isObject()) {
        setError(error, QStringLiteral("Claude credentials contain no Claude OAuth session"));
        return std::nullopt;
    }
    const QJsonObject oauth = oauthValue.toObject();
    const QString accessToken = nonEmpty(oauth.value(QStringLiteral("accessToken")));
    if (accessToken.isEmpty()) {
        setError(error, QStringLiteral("Claude OAuth access token is missing"));
        return std::nullopt;
    }
    const auto expiresAt = expiration(oauth.value(QStringLiteral("expiresAt")));
    if (!expiresAt) {
        setError(error, QStringLiteral("Claude OAuth expiry is invalid"));
        return std::nullopt;
    }

    QStringList scopes;
    const QJsonArray scopeValues = oauth.value(QStringLiteral("scopes")).toArray();
    for (const QJsonValue &scopeValue : scopeValues) {
        const QString scope = nonEmpty(scopeValue);
        if (!scope.isEmpty()) {
            scopes.append(scope);
        }
    }

    ClaudeCredentials credentials;
    credentials.accessToken = accessToken;
    credentials.refreshToken = nonEmpty(oauth.value(QStringLiteral("refreshToken")));
    credentials.expiresAt = *expiresAt;
    credentials.scopes = scopes;
    credentials.rateLimitTier = nonEmpty(oauth.value(QStringLiteral("rateLimitTier")));
    credentials.subscriptionType = nonEmpty(oauth.value(QStringLiteral("subscriptionType")));
    credentials.document = root;
    return credentials;
}

bool ClaudeCredentialStore::save(const QString &path, const ClaudeCredentials &credentials,
                                 QString *error)
{
    if (!validateExistingFile(QFileInfo(path), error)) {
        return false;
    }

    QJsonObject root = credentials.document;
    QJsonObject oauth = root.value(QStringLiteral("claudeAiOauth")).toObject();
    oauth.insert(QStringLiteral("accessToken"), credentials.accessToken);
    if (credentials.refreshToken.isEmpty()) {
        oauth.remove(QStringLiteral("refreshToken"));
    }
    else {
        oauth.insert(QStringLiteral("refreshToken"), credentials.refreshToken);
    }
    if (credentials.expiresAt.isValid()) {
        oauth.insert(QStringLiteral("expiresAt"), credentials.expiresAt.toMSecsSinceEpoch());
    }
    else {
        oauth.remove(QStringLiteral("expiresAt"));
    }
    QJsonArray scopes;
    for (const QString &scope : credentials.scopes) {
        scopes.append(scope);
    }
    oauth.insert(QStringLiteral("scopes"), scopes);
    if (!credentials.rateLimitTier.isEmpty()) {
        oauth.insert(QStringLiteral("rateLimitTier"), credentials.rateLimitTier);
    }
    if (!credentials.subscriptionType.isEmpty()) {
        oauth.insert(QStringLiteral("subscriptionType"), credentials.subscriptionType);
    }
    root.insert(QStringLiteral("claudeAiOauth"), oauth);

    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    // GCOVR_EXCL_START -- failures require concurrent path mutation or storage faults
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Claude credentials could not be opened for rotation"));
        return false;
    }
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        file.cancelWriting();
        setError(error, QStringLiteral("Claude credential permissions could not be secured"));
        return false;
    }
    const QByteArray output = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(output) != output.size() || !file.commit()) {
        setError(error, QStringLiteral("Claude credential rotation could not be committed"));
        return false;
    }
    // GCOVR_EXCL_STOP
    return true;
}

} // namespace Kodometer
