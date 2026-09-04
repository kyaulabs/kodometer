#pragma once

#include <QDateTime>
#include <QMap>
#include <QString>

#include <optional>

namespace Kodometer {

enum class KimiCredentialSource
{
    ApiKey,
    Cli
};

struct KimiCredentials
{
    QString accessToken;
    KimiCredentialSource source = KimiCredentialSource::ApiKey;
    QDateTime expiresAt;
    QString deviceId;
};

class KimiCredentialStore
{
  public:
    static constexpr qsizetype MaximumFileSize = 1024 * 1024;

    [[nodiscard]] static QString codeHomePath(const QMap<QString, QString> &environment = {},
                                              const QString &homeDirectory = {},
                                              const QString &workingDirectory = {});
    [[nodiscard]] static QString
    authenticationFilePath(const QMap<QString, QString> &environment = {},
                           const QString &homeDirectory = {}, const QString &workingDirectory = {});
    [[nodiscard]] static QString deviceFilePath(const QMap<QString, QString> &environment = {},
                                                const QString &homeDirectory = {},
                                                const QString &workingDirectory = {});
    [[nodiscard]] static std::optional<KimiCredentials> load(const QString &path,
                                                             QString *error = nullptr);
    [[nodiscard]] static std::optional<KimiCredentials> parse(const QByteArray &data,
                                                              QString *error = nullptr);
    [[nodiscard]] static std::optional<KimiCredentials>
    resolve(const QMap<QString, QString> &environment = {}, const QString &homeDirectory = {},
            const QString &workingDirectory = {},
            const QDateTime &now = QDateTime::currentDateTimeUtc(), QString *error = nullptr);
};

} // namespace Kodometer
