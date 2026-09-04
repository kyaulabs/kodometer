#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QString>

#include <optional>

namespace Kodometer {

enum class GeminiAuthType
{
    OAuthPersonal,
    ApiKey,
    VertexAi,
    Unknown
};

struct GeminiOAuthClient
{
    QString clientId;
    QString clientSecret;
};

struct GeminiCredentials
{
    QString accessToken;
    QString refreshToken;
    QString idToken;
    QDateTime expiresAt;
    QString email;
    QString hostedDomain;
    QJsonObject document;

    [[nodiscard]] bool needsRefresh(const QDateTime &now = QDateTime::currentDateTimeUtc()) const;
    [[nodiscard]] QString redactedEmail() const;
};

class GeminiCredentialStore
{
  public:
    static constexpr qsizetype MaximumFileSize = 1024 * 1024;

    [[nodiscard]] static QString authenticationFilePath(const QString &homeDirectory = {});
    [[nodiscard]] static QString settingsFilePath(const QString &homeDirectory = {});
    [[nodiscard]] static GeminiAuthType selectedAuthentication(const QString &settingsPath);
    [[nodiscard]] static std::optional<GeminiCredentials> load(const QString &path,
                                                               QString *error = nullptr);
    [[nodiscard]] static std::optional<GeminiCredentials> parse(const QByteArray &data,
                                                                QString *error = nullptr);
    [[nodiscard]] static bool save(const QString &path, const GeminiCredentials &credentials,
                                   QString *error = nullptr);
};

class GeminiOAuthConfig
{
  public:
    [[nodiscard]] static GeminiOAuthClient resolve(const QMap<QString, QString> &environment = {});
};

} // namespace Kodometer
