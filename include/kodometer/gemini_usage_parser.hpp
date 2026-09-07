#pragma once

#include <kodometer/gemini_credentials.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace Kodometer {

struct GeminiCodeAssistStatus
{
    QString tier;
    QString projectId;
    QString plan;
    bool consumerClientUnsupported = false;
};

class GeminiUsageParser
{
  public:
    [[nodiscard]] static std::optional<GeminiCodeAssistStatus>
    parseCodeAssist(const QByteArray &data, const QString &hostedDomain, QString *error = nullptr);
    [[nodiscard]] static std::optional<QVariantMap>
    parseQuota(const QByteArray &data, const GeminiCredentials &credentials,
               const GeminiCodeAssistStatus &status,
               const QDateTime &updatedAt = QDateTime::currentDateTimeUtc(),
               QString *error = nullptr);
    [[nodiscard]] static QString discoverProject(const QByteArray &data);
    [[nodiscard]] static bool isConsumerTierDeprecation(const QByteArray &data);
};

} // namespace Kodometer
