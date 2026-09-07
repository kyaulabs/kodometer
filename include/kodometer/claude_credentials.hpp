#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>

namespace Kodometer {

struct ClaudeCredentials
{
    QString accessToken;
    QString refreshToken;
    QDateTime expiresAt;
    QStringList scopes;
    QString rateLimitTier;
    QString subscriptionType;
    QJsonObject document;

    [[nodiscard]] bool needsRefresh(const QDateTime &now = QDateTime::currentDateTimeUtc()) const;
};

class ClaudeCredentialStore
{
  public:
    static constexpr qsizetype MaximumFileSize = 1024 * 1024;

    [[nodiscard]] static QString
    authenticationFilePath(const QMap<QString, QString> &environment = {},
                           const QString &homeDirectory = {}, const QString &workingDirectory = {});
    [[nodiscard]] static std::optional<ClaudeCredentials> load(const QString &path,
                                                               QString *error = nullptr);
    [[nodiscard]] static std::optional<ClaudeCredentials> parse(const QByteArray &data,
                                                                QString *error = nullptr);
    [[nodiscard]] static bool save(const QString &path, const ClaudeCredentials &credentials,
                                   QString *error = nullptr);
};

} // namespace Kodometer
