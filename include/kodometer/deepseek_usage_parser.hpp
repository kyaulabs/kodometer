#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace Kodometer {

class DeepSeekUsageParser
{
  public:
    [[nodiscard]] static std::optional<QVariantMap>
    parse(const QByteArray &data, const QDateTime &updatedAt, QString *error = nullptr);
};

} // namespace Kodometer
