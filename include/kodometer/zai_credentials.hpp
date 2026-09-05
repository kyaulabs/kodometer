#pragma once

#include <QMap>
#include <QString>

#include <optional>

namespace Kodometer {

enum class ZaiRegion
{
    Global,
    BigModelChina
};

enum class ZaiUsageScope
{
    Personal,
    Team
};

enum class ZaiCredentialSource
{
    Environment,
    CredentialFile
};

struct ZaiCredentials
{
    QString apiKey;
    ZaiRegion region = ZaiRegion::Global;
    ZaiUsageScope scope = ZaiUsageScope::Personal;
    ZaiCredentialSource source = ZaiCredentialSource::Environment;
    QString organizationId;
    QString projectId;
};

class ZaiCredentialResolver
{
  public:
    static constexpr qsizetype MaximumCredentialFileSize = 1024 * 1024;

    [[nodiscard]] static std::optional<ZaiCredentials>
    resolve(const QMap<QString, QString> &environment, const QString &homeDirectory = {},
            QString *error = nullptr);
};

} // namespace Kodometer
