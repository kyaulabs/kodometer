#pragma once

#include <QMap>
#include <QString>

#include <optional>

namespace Kodometer {

struct XaiCredentials
{
    QString managementApiKey;
    QString teamId;
};

class XaiCredentialResolver
{
  public:
    [[nodiscard]] static std::optional<XaiCredentials>
    resolve(const QMap<QString, QString> &environment, QString *error = nullptr);
};

} // namespace Kodometer
