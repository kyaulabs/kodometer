#pragma once

#include <kodometer/claude_credentials.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace Kodometer {

class ClaudeUsageParser
{
  public:
    [[nodiscard]] static std::optional<QVariantMap>
    parse(const QByteArray &data, const ClaudeCredentials &credentials,
          const QDateTime &updatedAt = QDateTime::currentDateTimeUtc(), QString *error = nullptr);
    [[nodiscard]] static QString formatPlan(const QString &subscriptionType,
                                            const QString &rateLimitTier);
};

} // namespace Kodometer
