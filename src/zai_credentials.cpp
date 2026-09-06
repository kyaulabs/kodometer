#include <kodometer/zai_credentials.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif

namespace Kodometer {
namespace {

void setError(QString *error, const QString &message)
{
    // GCOVR_EXCL_BR_START -- tested error output uses Qt pointer and assignment branches
    if (error != nullptr) {
        *error = message;
    }
    // GCOVR_EXCL_BR_STOP
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

bool containsHeaderBreak(const QString &value)
{
    // GCOVR_EXCL_BR_START -- carriage-return and newline inputs are tested
    const bool contains = value.contains(QLatin1Char('\r')) || value.contains(QLatin1Char('\n'));
    // GCOVR_EXCL_BR_STOP
    return contains;
}

bool permissionsExposeSecret(QFileDevice::Permissions permissions)
{
    constexpr auto exposed = QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                             QFileDevice::ReadOther | QFileDevice::WriteOther;
    return (permissions & exposed) != QFileDevice::Permissions{};
}

std::optional<QString> credentialFile(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (info.isSymbolicLink()) {
        setError(error, QStringLiteral("z.ai credential file must not be a symbolic link"));
        return std::nullopt;
    }
    if (!info.exists()) {
        return std::nullopt;
    }
    if (!info.isFile()) {
        setError(error, QStringLiteral("z.ai credential path is not a regular file"));
        return std::nullopt;
    }
#if defined(Q_OS_UNIX)
    if (info.ownerId() != static_cast<uint>(geteuid())) { // GCOVR_EXCL_BR_LINE
        setError(error, QStringLiteral("z.ai credential file is owned by another user"));
        return std::nullopt;
    }
#endif
    if (permissionsExposeSecret(info.permissions())) {
        setError(error, QStringLiteral("z.ai credential file permissions are too broad"));
        return std::nullopt;
    }
    if (info.size() > ZaiCredentialResolver::MaximumCredentialFileSize) {
        setError(error, QStringLiteral("z.ai credential file exceeds the 1 MiB limit"));
        return std::nullopt;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("z.ai credential file could not be read"));
        return std::nullopt;
    }
    const QByteArray data = file.read(ZaiCredentialResolver::MaximumCredentialFileSize + 1);
    // GCOVR_EXCL_BR_START -- bounds a file replaced after QFileInfo validation
    if (data.size() > ZaiCredentialResolver::MaximumCredentialFileSize) {
        setError(error, QStringLiteral("z.ai credential file exceeds the 1 MiB limit"));
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_STOP
    const QString firstLine = QString::fromUtf8(data).section(QLatin1Char('\n'), 0, 0);
    const QString key = cleaned(firstLine);
    if (key.isEmpty()) {
        setError(error, QStringLiteral("z.ai credential file contains no API key"));
        return std::nullopt;
    }
    if (containsHeaderBreak(key)) {
        setError(error, QStringLiteral("z.ai API key contains invalid characters"));
        return std::nullopt;
    }
    return key;
}

} // namespace

bool ZaiCredentialResolver::validAccountOptions(const QVariantMap &options)
{
    for (const QVariant &value : options) {
        if (value.metaType().id() != QMetaType::QString)
            return false;
    }
    const QString region = options.value(QStringLiteral("region")).toString();
    const QString scope = options.value(QStringLiteral("scope")).toString();
    if (region != QLatin1String("global") && region != QLatin1String("bigmodel-cn"))
        return false;
    if (scope == QLatin1String("personal"))
        return options.size() == 2;
    if (scope != QLatin1String("team") || options.size() != 4)
        return false;
    static const QRegularExpression identifier(QStringLiteral("\\A[A-Za-z0-9_-]{1,256}\\z"));
    const QString organization = options.value(QStringLiteral("organizationId")).toString();
    const QString project = options.value(QStringLiteral("projectId")).toString();
    const bool validOrganization = identifier.match(organization).hasMatch();
    const bool validProject = identifier.match(project).hasMatch();
    return validOrganization && validProject;
}

std::optional<ZaiCredentials>
ZaiCredentialResolver::resolveNamed(const QString &key, const QVariantMap &options, QString *error)
{
    if (key.isEmpty() || key.size() > 65536 || !std::all_of(key.cbegin(), key.cend(), [](QChar ch) {
            return ch.unicode() >= 0x21 && ch.unicode() <= 0x7e;
        })) {
        setError(error, QStringLiteral("Selected z.ai account has an invalid API key"));
        return std::nullopt;
    }
    if (!validAccountOptions(options)) {
        setError(error,
                 QStringLiteral("Selected z.ai account has invalid region or scope settings"));
        return std::nullopt;
    }
    ZaiCredentials credentials;
    credentials.apiKey = key;
    credentials.source = ZaiCredentialSource::WalletAccount;
    credentials.region =
        options.value(QStringLiteral("region")).toString() == QLatin1String("bigmodel-cn")
            ? ZaiRegion::BigModelChina
            : ZaiRegion::Global;
    credentials.scope = options.value(QStringLiteral("scope")).toString() == QLatin1String("team")
                            ? ZaiUsageScope::Team
                            : ZaiUsageScope::Personal;
    credentials.organizationId = options.value(QStringLiteral("organizationId")).toString();
    credentials.projectId = options.value(QStringLiteral("projectId")).toString();
    return credentials;
}

std::optional<ZaiCredentials>
ZaiCredentialResolver::resolve(const QMap<QString, QString> &environment,
                               const QString &homeDirectory, QString *error)
{
    const QString regionName = cleaned(environment.value(QStringLiteral("Z_AI_REGION"))).toLower();
    ZaiRegion region = ZaiRegion::Global;
    if (!regionName.isEmpty() && regionName != QStringLiteral("global")) {
        if (regionName != QStringLiteral("bigmodel-cn")) {
            setError(error, QStringLiteral("z.ai region must be global or bigmodel-cn"));
            return std::nullopt;
        }
        region = ZaiRegion::BigModelChina;
    }

    const QString scopeName =
        cleaned(environment.value(QStringLiteral("Z_AI_USAGE_SCOPE"))).toLower();
    ZaiUsageScope scope = ZaiUsageScope::Personal;
    if (!scopeName.isEmpty() && scopeName != QStringLiteral("personal")) {
        if (scopeName != QStringLiteral("team")) {
            setError(error, QStringLiteral("z.ai usage scope must be personal or team"));
            return std::nullopt;
        }
        scope = ZaiUsageScope::Team;
    }

    QString apiKey = cleaned(environment.value(QStringLiteral("Z_AI_API_KEY")));
    ZaiCredentialSource source = ZaiCredentialSource::Environment;
    if (apiKey.isEmpty() && region == ZaiRegion::BigModelChina) {
        for (const QString &name :
             {QStringLiteral("BIGMODEL_API_KEY"), QStringLiteral("ZHIPU_API_KEY"),
              QStringLiteral("ZHIPUAI_API_KEY"), QStringLiteral("GLM_API_KEY")}) {
            apiKey = cleaned(environment.value(name));
            if (!apiKey.isEmpty()) {
                break;
            }
        }
    }
    if (apiKey.isEmpty() && region == ZaiRegion::BigModelChina) {
        // GCOVR_EXCL_BR_START -- Qt path and provider-file iteration branches
        const QString home = homeDirectory.isEmpty() ? QDir::homePath() : homeDirectory;
        for (const QString &relative : {QStringLiteral(".config/bigmodel/api_key"),
                                        QStringLiteral(".config/zhipu/api_key")}) {
            QString fileError;
            const auto key = credentialFile(QDir(home).filePath(relative), &fileError);
            if (key) {
                apiKey = *key;
                source = ZaiCredentialSource::CredentialFile;
                break;
            }
            if (!fileError.isEmpty()) {
                setError(error, fileError);
                return std::nullopt;
            }
        }
        // GCOVR_EXCL_BR_STOP
    }
    if (apiKey.isEmpty()) {
        setError(error, QStringLiteral("z.ai API key is missing"));
        return std::nullopt;
    }
    if (containsHeaderBreak(apiKey)) {
        setError(error, QStringLiteral("z.ai API key contains invalid characters"));
        return std::nullopt;
    }

    QString organization = cleaned(environment.value(QStringLiteral("Z_AI_BIGMODEL_ORGANIZATION")));
    QString project = cleaned(environment.value(QStringLiteral("Z_AI_BIGMODEL_PROJECT")));
    if (organization.isEmpty()) {
        organization = cleaned(environment.value(QStringLiteral("Z_AI_ORGANIZATION")));
    }
    if (project.isEmpty()) {
        project = cleaned(environment.value(QStringLiteral("Z_AI_PROJECT")));
    }
    if (scope == ZaiUsageScope::Team) {
        if (organization.isEmpty() || project.isEmpty()) {
            setError(error, QStringLiteral(
                                "z.ai team usage requires organization and project identifiers"));
            return std::nullopt;
        }
        if (containsHeaderBreak(organization) || containsHeaderBreak(project)) {
            setError(error, QStringLiteral("z.ai team identifiers contain invalid characters"));
            return std::nullopt;
        }
    }
    else {
        organization.clear();
        project.clear();
    }

    // GCOVR_EXCL_BR_START -- aggregate construction carries QString allocation branches
    const ZaiCredentials credentials{apiKey, region, scope, source, organization, project};
    // GCOVR_EXCL_BR_STOP
    return credentials;
}

} // namespace Kodometer
