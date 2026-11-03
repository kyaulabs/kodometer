#include <kodometer/openai_costs.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <cmath>

namespace Kodometer {
std::optional<QVariantMap> OpenAICosts::parse(const QByteArray &bytes, qint64 startTime,
                                              const QDateTime &now)
{
    const auto document = QJsonDocument::fromJson(bytes);
    if (!document.isObject() || !now.isValid())
        return std::nullopt;
    const auto root = document.object();
    const auto data = root.value(QStringLiteral("data"));
    const auto more = root.value(QStringLiteral("has_more"));
    if (root.value(QStringLiteral("object")) != QLatin1String("page") || !data.isArray() ||
        !more.isBool() || more.toBool() || !root.value(QStringLiteral("next_page")).isNull())
        return std::nullopt;
    const auto buckets = data.toArray();
    if (buckets.size() > 62)
        return std::nullopt;
    QVariantList daily;
    QSet<qint64> seen;
    double month = 0;
    double total = 0;
    bool monthReported = false;
    bool totalReported = false;
    std::optional<double> today;
    const QDate date = now.toUTC().date();
    for (const auto &value : buckets) {
        const auto bucket = value.toObject();
        const auto start = bucket.value(QStringLiteral("start_time"));
        const auto end = bucket.value(QStringLiteral("end_time"));
        const double seconds = start.toDouble(-1);
        if (!start.isDouble() || !end.isDouble() || !std::isfinite(seconds) ||
            seconds < static_cast<double>(startTime) ||
            seconds >= static_cast<double>(now.toSecsSinceEpoch()) ||
            std::fmod(seconds, 86400) != 0 || end.toDouble() != seconds + 86400)
            return std::nullopt;
        const auto timestamp = static_cast<qint64>(seconds);
        const auto results = bucket.value(QStringLiteral("results"));
        if (bucket.value(QStringLiteral("object")) != QLatin1String("bucket") ||
            !results.isArray() || seen.contains(timestamp))
            return std::nullopt;
        seen.insert(timestamp);
        double spent = 0;
        const auto rows = results.toArray();
        // Without group_by, the API returns at most one aggregate per bucket.
        if (rows.size() > 1)
            return std::nullopt;
        if (rows.isEmpty())
            continue; // No reported amount is not evidence of zero spending.
        const auto row = rows.first().toObject();
        const auto amount = row.value(QStringLiteral("amount")).toObject();
        const auto number = amount.value(QStringLiteral("value"));
        spent = number.toDouble(-1);
        if (row.value(QStringLiteral("object")) != QLatin1String("organization.costs.result") ||
            amount.value(QStringLiteral("currency")) != QLatin1String("usd") ||
            !number.isDouble() || !std::isfinite(spent) || spent < 0)
            return std::nullopt;
        const auto day = QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::UTC).date();
        daily.append(QVariantMap{{QStringLiteral("label"), day.toString(Qt::ISODate)},
                                 {QStringLiteral("value"), spent}});
        if (day.year() == date.year() && day.month() == date.month()) {
            month += spent;
            monthReported = true;
        }
        if (day >= date.addDays(-29)) {
            total += spent;
            totalReported = true;
        }
        if (!std::isfinite(month) || !std::isfinite(total))
            return std::nullopt;
        if (day == date)
            today = spent;
    }
    QVariantMap cost{{QStringLiteral("daily"), daily},
                     {QStringLiteral("currencyCode"), QStringLiteral("USD")},
                     {QStringLiteral("historyPartial"), true},
                     {QStringLiteral("historyEndDate"), date.toString(Qt::ISODate)},
                     {QStringLiteral("historyIncludesCurrentDay"), true}};
    if (monthReported)
        cost.insert(QStringLiteral("monthToDateUSD"), month);
    if (totalReported)
        cost.insert(QStringLiteral("last30DaysUSD"), total);
    if (today)
        cost.insert(QStringLiteral("todayUSD"), *today);
    return QVariantMap{{QStringLiteral("cost"), cost},
                       {QStringLiteral("balanceUnavailable"), true}};
}
} // namespace Kodometer
