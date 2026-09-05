#pragma once

#include <kodometer/zai_credentials.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace Kodometer {

struct ZaiModelUsage
{
    QVariantList points;
    QVariantList rows;
};

struct ZaiBalance
{
    double available = 0.0;
    double recharged = 0.0;
    double granted = 0.0;
    double spent = 0.0;
};

class ZaiUsageParser
{
  public:
    [[nodiscard]] static std::optional<QVariantMap>
    parseQuota(const QByteArray &data, ZaiRegion region, ZaiUsageScope scope,
               const QDateTime &updatedAt, QString *error = nullptr);
    [[nodiscard]] static std::optional<ZaiModelUsage> parseModelUsage(const QByteArray &data,
                                                                      QString *error = nullptr);
    [[nodiscard]] static std::optional<ZaiBalance> parseBalance(const QByteArray &data,
                                                                QString *error = nullptr);

    static void appendModelUsage(QVariantMap &provider, const ZaiModelUsage &usage,
                                 const QString &title);
    static void appendBalance(QVariantMap &provider, const ZaiBalance &balance);
};

} // namespace Kodometer
