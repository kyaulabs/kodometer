#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace Kodometer {

struct OpenRouterCredits
{
    double totalCredits = 0.0;
    double totalUsage = 0.0;
    double balance = 0.0;
};

struct OpenRouterKeyUsage
{
    std::optional<double> limit;
    std::optional<double> limitRemaining;
    QString limitReset;
    std::optional<double> usage;
    std::optional<double> daily;
    std::optional<double> weekly;
    std::optional<double> monthly;
    std::optional<qint64> rateLimitRequests;
    QString rateLimitInterval;
};

struct OpenRouterActivityEntry
{
    QString date;
    QString model;
    QString endpointId;
    QString providerName;
    QString workspaceId;
    qint64 inputTokens = 0;
    qint64 outputTokens = 0;
    qint64 reasoningTokens = 0;
    qint64 requests = 0;
    double cost = 0.0;
    double estimatedCost = 0.0;
};

struct OpenRouterActivity
{
    QList<OpenRouterActivityEntry> entries;
    qint64 inputTokens = 0;
    qint64 outputTokens = 0;
    qint64 reasoningTokens = 0;
    qint64 requests = 0;
    double totalCost = 0.0;
    double estimatedCost = 0.0;
};

class OpenRouterUsageParser
{
  public:
    [[nodiscard]] static std::optional<OpenRouterCredits> parseCredits(const QByteArray &data,
                                                                       QString *error = nullptr);
    [[nodiscard]] static std::optional<OpenRouterKeyUsage> parseKey(const QByteArray &data,
                                                                    QString *error = nullptr);
    [[nodiscard]] static std::optional<OpenRouterActivity>
    parseActivity(const QByteArray &data, const QDateTime &updatedAt, QString *error = nullptr);
    [[nodiscard]] static std::optional<OpenRouterActivity>
    mergeActivity(const OpenRouterActivity &first, const OpenRouterActivity &second,
                  QString *error = nullptr);

    [[nodiscard]] static QVariantMap
    provider(const OpenRouterCredits &credits,
             const std::optional<OpenRouterKeyUsage> &keyUsage = std::nullopt,
             const QString &keyDiagnostic = {}, const QString &activityDiagnostic = {},
             const QDateTime &updatedAt = QDateTime::currentDateTimeUtc(),
             const std::optional<OpenRouterActivity> &activity = std::nullopt);
};

} // namespace Kodometer
