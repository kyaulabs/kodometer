#pragma once

#include <QMap>
#include <QString>

#include <optional>

namespace Kodometer {

struct DeepSeekCredentials
{
    QString apiKey;
};

class DeepSeekCredentialResolver
{
  public:
    [[nodiscard]] static std::optional<DeepSeekCredentials>
    resolve(const QMap<QString, QString> &environment, QString *error = nullptr);
};

} // namespace Kodometer
