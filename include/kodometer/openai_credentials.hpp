#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QString>

#include <optional>

namespace Kodometer {

struct OpenAICredentials
{
    QString accessToken;
    QString refreshToken;
    QString idToken;
    QString accountId;
    QString email;
    QDateTime lastRefresh;
    QDateTime expiresAt;
    bool apiKey = false;
    QJsonObject document;

    [[nodiscard]] bool needsRefresh(const QDateTime &now = QDateTime::currentDateTimeUtc()) const;
    [[nodiscard]] QString redactedEmail() const;
};

class OpenAICredentialStore
{
  public:
    static constexpr qint64 MaximumFileSize = 1024 * 1024;

    [[nodiscard]] static QString
    authenticationFilePath(const QMap<QString, QString> &environment = {},
                           const QString &homeDirectory = {});
    [[nodiscard]] static std::optional<OpenAICredentials> load(const QString &path,
                                                               QString *error = nullptr);
    [[nodiscard]] static std::optional<OpenAICredentials> parse(const QByteArray &data,
                                                                QString *error = nullptr);
    [[nodiscard]] static bool save(const QString &path, const OpenAICredentials &credentials,
                                   QString *error = nullptr);

  private:
    OpenAICredentialStore() = delete;
};

} // namespace Kodometer
