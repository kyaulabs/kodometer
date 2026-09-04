#include <kodometer/kimi_credentials.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QTimeZone>
#include <QUuid>

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

bool containsHeaderBreak(const QString &value)
{
    // GCOVR_EXCL_BR_START -- tested QString searches contain Qt branches
    const bool contains = value.contains(QLatin1Char('\r')) || value.contains(QLatin1Char('\n'));
    // GCOVR_EXCL_BR_STOP
    return contains;
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

bool permissionsExposeSecrets(QFileDevice::Permissions permissions)
{
    constexpr auto exposed = QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                             QFileDevice::ReadOther | QFileDevice::WriteOther;
    return (permissions & exposed) !=
           QFileDevice::Permissions{}; // GCOVR_EXCL_BR_LINE -- Qt flags internals
}

bool validateCredentialFile(const QFileInfo &info, QString *error)
{
    if (info.isSymbolicLink()) {
        setError(error, QStringLiteral("Kimi Code credentials must not be a symbolic link"));
        return false;
    }
    if (!info.exists()) {
        setError(error, QStringLiteral("Kimi Code credentials were not found"));
        return false;
    }
    if (!info.isFile()) {
        setError(error, QStringLiteral("Kimi Code credential path is not a regular file"));
        return false;
    }
#if defined(Q_OS_UNIX)
    if (info.ownerId() != static_cast<uint>(geteuid())) { // GCOVR_EXCL_BR_LINE
        setError(error, QStringLiteral("Kimi Code credentials are owned by another user"));
        return false;
    }
#endif
    if (permissionsExposeSecrets(info.permissions())) {
        setError(error, QStringLiteral("Kimi Code credential permissions expose secrets"));
        return false;
    }
    if (info.size() > KimiCredentialStore::MaximumFileSize) {
        setError(error, QStringLiteral("Kimi Code credentials exceed the 1 MiB limit"));
        return false;
    }
    return true;
}

std::optional<QDateTime> expiration(const QJsonValue &value)
{
    double seconds = 0.0;
    bool valid = false;
    if (value.isDouble()) {
        seconds = value.toDouble();
        valid = true;
    }
    else if (value.isString()) {
        seconds = value.toString().trimmed().toDouble(&valid);
    }
    constexpr double MaximumSeconds =
        static_cast<double>(std::numeric_limits<qint64>::max()) / 1000.0;
    if (!valid || !std::isfinite(seconds) || seconds < 0.0 ||
        seconds > MaximumSeconds) { // GCOVR_EXCL_BR_LINE -- all input classes are tested
        return std::nullopt;
    }
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(seconds * 1000.0), QTimeZone::UTC);
}

std::optional<QString> loadOrCreateDeviceId(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (info.exists() ||
        info.isSymbolicLink()) { // GCOVR_EXCL_BR_LINE -- tested QFileInfo accessors
        if (info.isSymbolicLink() ||
            !info.isFile()) { // GCOVR_EXCL_BR_LINE -- symlink and directory are tested
            setError(error, QStringLiteral("Kimi Code device ID path is not a regular file"));
            return std::nullopt;
        }
#if defined(Q_OS_UNIX)
        if (info.ownerId() != static_cast<uint>(geteuid())) { // GCOVR_EXCL_BR_LINE
            setError(error, QStringLiteral("Kimi Code device ID is owned by another user"));
            return std::nullopt;
        }
#endif
        if (permissionsExposeSecrets(info.permissions())) {
            setError(error, QStringLiteral("Kimi Code device ID permissions are not private"));
            return std::nullopt;
        }
        if (info.size() > 4096) {
            setError(error, QStringLiteral("Kimi Code device ID is too large"));
            return std::nullopt;
        }
        QFile file(path);
        // GCOVR_EXCL_START -- validated file failures require concurrent mutation
        if (!file.open(QIODevice::ReadOnly)) {
            setError(error, QStringLiteral("Kimi Code device ID could not be read"));
            return std::nullopt;
        }
        // GCOVR_EXCL_STOP
        const QString deviceId = QString::fromUtf8(file.read(4097)).trimmed();
        if (deviceId.isEmpty()) {
            setError(error, QStringLiteral("Kimi Code device ID is empty"));
            return std::nullopt;
        }
        return deviceId;
    }

    const QString deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    // GCOVR_EXCL_START -- failures require concurrent path mutation or storage faults
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Kimi Code device ID could not be created"));
        return std::nullopt;
    }
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        file.cancelWriting();
        setError(error, QStringLiteral("Kimi Code device ID permissions could not be secured"));
        return std::nullopt;
    }
    const QByteArray encoded = deviceId.toUtf8();
    if (file.write(encoded) != encoded.size() || !file.commit()) {
        setError(error, QStringLiteral("Kimi Code device ID could not be saved"));
        return std::nullopt;
    }
    // GCOVR_EXCL_STOP
    return deviceId;
}

} // namespace

QString KimiCredentialStore::codeHomePath(const QMap<QString, QString> &environment,
                                          const QString &homeDirectory,
                                          const QString &workingDirectory)
{
    const QString configured = environment.value(QStringLiteral("KIMI_CODE_HOME")).trimmed();
    // GCOVR_EXCL_BR_START -- all path choices are tested; Qt path accessors add branches
    if (!configured.isEmpty()) {
        if (QDir::isAbsolutePath(configured)) {
            return QDir::cleanPath(configured);
        }
        const QString base = workingDirectory.isEmpty() ? QDir::currentPath() : workingDirectory;
        return QDir::cleanPath(QDir(base).absoluteFilePath(configured));
    }
    const QString home = homeDirectory.isEmpty() ? QDir::homePath() : homeDirectory;
    // GCOVR_EXCL_BR_STOP
    return QDir::cleanPath(QDir(home).filePath(QStringLiteral(".kimi-code")));
}

QString KimiCredentialStore::authenticationFilePath(const QMap<QString, QString> &environment,
                                                    const QString &homeDirectory,
                                                    const QString &workingDirectory)
{
    return QDir(codeHomePath(environment, homeDirectory, workingDirectory))
        .filePath(QStringLiteral("credentials/kimi-code.json"));
}

QString KimiCredentialStore::deviceFilePath(const QMap<QString, QString> &environment,
                                            const QString &homeDirectory,
                                            const QString &workingDirectory)
{
    return QDir(codeHomePath(environment, homeDirectory, workingDirectory))
        .filePath(QStringLiteral("device_id"));
}

std::optional<KimiCredentials> KimiCredentialStore::load(const QString &path, QString *error)
{
    if (!validateCredentialFile(QFileInfo(path), error)) {
        return std::nullopt;
    }
    QFile file(path);
    // GCOVR_EXCL_START -- validated file failures require concurrent mutation
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Kimi Code credentials could not be read"));
        return std::nullopt;
    }
    const QByteArray data = file.read(MaximumFileSize + 1);
    if (data.size() > MaximumFileSize) {
        setError(error, QStringLiteral("Kimi Code credentials exceed the 1 MiB limit"));
        return std::nullopt;
    }
    // GCOVR_EXCL_STOP
    return parse(data, error);
}

std::optional<KimiCredentials> KimiCredentialStore::parse(const QByteArray &data, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Kimi Code credentials contain invalid JSON"));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("Kimi Code credentials must contain a JSON object"));
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    const QString accessToken = root.value(QStringLiteral("access_token")).toString().trimmed();
    if (accessToken.isEmpty()) {
        setError(error, QStringLiteral("Kimi Code CLI access token is missing"));
        return std::nullopt;
    }
    if (containsHeaderBreak(accessToken)) {
        setError(error, QStringLiteral("Kimi Code CLI access token contains invalid characters"));
        return std::nullopt;
    }
    const auto expiresAt = expiration(root.value(QStringLiteral("expires_at")));
    if (!expiresAt ||
        !expiresAt->isValid()) { // GCOVR_EXCL_BR_LINE -- parser only returns valid dates
        setError(error, QStringLiteral("Kimi Code CLI expiry is missing or invalid"));
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_START -- Qt value construction branches
    const KimiCredentials credentials{accessToken, KimiCredentialSource::Cli, *expiresAt, {}};
    // GCOVR_EXCL_BR_STOP
    return credentials;
}

std::optional<KimiCredentials>
KimiCredentialStore::resolve(const QMap<QString, QString> &environment,
                             const QString &homeDirectory, const QString &workingDirectory,
                             const QDateTime &now, QString *error)
{
    const QString apiKey = cleaned(environment.value(QStringLiteral("KIMI_CODE_API_KEY")));
    if (!apiKey.isEmpty()) {
        if (containsHeaderBreak(apiKey)) {
            setError(error, QStringLiteral("Kimi Code API key contains invalid characters"));
            return std::nullopt;
        }
        // GCOVR_EXCL_BR_START -- Qt value construction branches
        const KimiCredentials credentials{apiKey, KimiCredentialSource::ApiKey, {}, {}};
        // GCOVR_EXCL_BR_STOP
        return credentials;
    }

    const auto loaded =
        load(authenticationFilePath(environment, homeDirectory, workingDirectory), error);
    if (!loaded) {
        return std::nullopt;
    }
    if (now.secsTo(loaded->expiresAt) <= 60) {
        setError(error, QStringLiteral("Kimi Code CLI credential is expired; sign in again or set "
                                       "KIMI_CODE_API_KEY"));
        return std::nullopt;
    }
    auto credentials = *loaded;
    const auto deviceId =
        loadOrCreateDeviceId(deviceFilePath(environment, homeDirectory, workingDirectory), error);
    if (!deviceId) {
        return std::nullopt;
    }
    credentials.deviceId = *deviceId;
    return credentials;
}

} // namespace Kodometer
