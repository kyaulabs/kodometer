#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QTimeZone>
#include <QVariantMap>
#include <optional>

namespace Kodometer {
class OpenAICosts
{
  public:
    // Requires a complete, ungrouped daily organization Costs response.
    [[nodiscard]] static std::optional<QVariantMap> parse(const QByteArray &bytes, qint64 startTime,
                                                          const QDateTime &now);
};
} // namespace Kodometer
