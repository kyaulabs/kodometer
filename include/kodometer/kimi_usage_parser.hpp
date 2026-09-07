#pragma once

#include <kodometer/kimi_credentials.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace Kodometer {

class KimiUsageParser
{
  public:
    [[nodiscard]] static std::optional<QVariantMap> parse(const QByteArray &data,
                                                          KimiCredentialSource source,
                                                          const QDateTime &updatedAt,
                                                          QString *error = nullptr);
};

} // namespace Kodometer
