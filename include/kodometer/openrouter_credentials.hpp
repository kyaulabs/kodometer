#pragma once

#include <QMap>
#include <QString>

#include <optional>

namespace Kodometer {

struct OpenRouterCredentials
{
    QString apiKey;
    QString managementApiKey;
    QString httpReferer;
    QString clientTitle;
};

class OpenRouterCredentialResolver
{
  public:
    [[nodiscard]] static std::optional<OpenRouterCredentials>
    resolve(const QMap<QString, QString> &environment, QString *error = nullptr);
};

} // namespace Kodometer
