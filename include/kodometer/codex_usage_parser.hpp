#pragma once

#include <kodometer/codex_credentials.hpp>

#include <QDateTime>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace Kodometer {

class CodexUsageParser
{
  public:
    [[nodiscard]] static std::optional<QVariantMap> parse(const QByteArray &data,
                                                          const CodexCredentials &credentials,
                                                          const QDateTime &updatedAt,
                                                          QString *error = nullptr);
    [[nodiscard]] static QString formatPlan(const QString &plan);

  private:
    CodexUsageParser() = delete;
};

} // namespace Kodometer
