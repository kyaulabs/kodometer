#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QMap>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace Kodometer {

struct XaiBalance
{
    double balanceUsd = 0.0;
};

struct XaiUsageHistory
{
    QMap<QString, double> daily;
    bool partial = false;
};

class XaiUsageParser
{
  public:
    [[nodiscard]] static std::optional<XaiBalance> parseBalance(const QByteArray &data,
                                                                QString *error = nullptr);
    [[nodiscard]] static std::optional<XaiUsageHistory> parseHistory(const QByteArray &data,
                                                                     QString *error = nullptr);
    [[nodiscard]] static QVariantMap
    provider(const XaiBalance &balance, const std::optional<XaiUsageHistory> &history,
             const QDateTime &updatedAt = QDateTime::currentDateTimeUtc());
};

} // namespace Kodometer
