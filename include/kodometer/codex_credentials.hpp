#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QString>

#include <optional>

namespace Kodometer {

struct CodexCredentials
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

class CodexCredentialStore
{
  public:
    static constexpr qint64 MaximumFileSize = 1024 * 1024;

    [[nodiscard]] static QString
    authenticationFilePath(const QMap<QString, QString> &environment = {},
                           const QString &homeDirectory = {});
    [[nodiscard]] static std::optional<CodexCredentials> load(const QString &path,
                                                              QString *error = nullptr);
    [[nodiscard]] static std::optional<CodexCredentials> parse(const QByteArray &data,
                                                               QString *error = nullptr);
    [[nodiscard]] static bool save(const QString &path, const CodexCredentials &credentials,
                                   QString *error = nullptr);

  private:
    CodexCredentialStore() = delete;
};

} // namespace Kodometer
