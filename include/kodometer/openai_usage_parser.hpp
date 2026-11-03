#pragma once

#include <kodometer/openai_credentials.hpp>

#include <QDateTime>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace Kodometer {

class OpenAIUsageParser
{
  public:
    [[nodiscard]] static std::optional<QVariantMap> parse(const QByteArray &data,
                                                          const OpenAICredentials &credentials,
                                                          const QDateTime &updatedAt,
                                                          QString *error = nullptr);
    [[nodiscard]] static QString formatPlan(const QString &plan);

  private:
    OpenAIUsageParser() = delete;
};

} // namespace Kodometer
