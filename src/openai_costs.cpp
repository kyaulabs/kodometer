#include <kodometer/openai_costs.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <cmath>

namespace Kodometer {
namespace {
std::optional<QVariantMap> parseResponse(const QByteArray &bytes, qint64 startTime,
                                         const QDateTime &now, bool usage)
{
    const auto document = QJsonDocument::fromJson(bytes);
    if (!document.isObject() || !now.isValid())
        return std::nullopt;
    const auto root = document.object();
    const auto typeValue = root.value(QStringLiteral("object"));
    const QString type = typeValue.toString();
    const auto data = root.value(QStringLiteral("data"));
    const auto more = root.value(QStringLiteral("has_more"));
    const auto next = root.value(QStringLiteral("next_page"));
    if (type != QLatin1String("page") || !data.isArray() || !more.isBool() || more.toBool() ||
        !next.isNull())
        return std::nullopt;
    const auto buckets = data.toArray();
    if (buckets.size() > (usage ? 31 : 62))
        return std::nullopt;
    QVariantList daily;
    QSet<qint64> seen;
    double inputTokens = 0;
    double cachedTokens = 0;
    double month = 0;
    double total = 0;
    bool monthReported = false;
    bool totalReported = false;
    std::optional<double> today;
    const auto utc = now.toUTC();
    const QDate date = utc.date();
    const qint64 endTime = now.toSecsSinceEpoch();
    for (const auto &value : buckets) {
        const auto bucket = value.toObject();
        const auto start = bucket.value(QStringLiteral("start_time"));
        const auto end = bucket.value(QStringLiteral("end_time"));
        const double seconds = start.toDouble(-1);
        const double endSeconds = end.toDouble(-1);
        if (!start.isDouble() || !end.isDouble() || !std::isfinite(seconds) ||
            seconds < static_cast<double>(startTime) || seconds >= static_cast<double>(endTime) ||
            std::fmod(seconds, 86400) != 0 ||
            (endSeconds != seconds + 86400 &&
             (endSeconds != static_cast<double>(endTime) || endSeconds > seconds + 86400)))
            return std::nullopt;
        const auto timestamp = static_cast<qint64>(seconds);
        const auto bucketTypeValue = bucket.value(QStringLiteral("object"));
        const QString bucketType = bucketTypeValue.toString();
        const auto results = bucket.value(QStringLiteral("results"));
        if (bucketType != QLatin1String("bucket") || !results.isArray() || seen.contains(timestamp))
            return std::nullopt;
        seen.insert(timestamp);
        const auto rows = results.toArray();
        // Without group_by, the API returns at most one aggregate per bucket.
        if (rows.size() > 1)
            return std::nullopt;
        if (rows.isEmpty())
            continue; // No reported amount is not evidence of zero spending.
        const auto row = rows.first().toObject();
        const auto rowTypeValue = row.value(QStringLiteral("object"));
        const QString rowType = rowTypeValue.toString();
        if (usage) {
            const auto input = row.value(QStringLiteral("input_tokens"));
            const auto cached = row.value(QStringLiteral("input_cached_tokens"));
            const double inputCount = input.toDouble(-1);
            const double cachedCount = cached.toDouble(-1);
            constexpr double maximumExactInteger = 9007199254740991.0;
            if (rowType != QLatin1String("organization.usage.completions.result") ||
                !input.isDouble() || !cached.isDouble() || inputCount < 0 || cachedCount < 0 ||
                std::floor(inputCount) != inputCount || std::floor(cachedCount) != cachedCount ||
                cachedCount > inputCount || inputCount > maximumExactInteger - inputTokens)
                return std::nullopt;
            inputTokens += inputCount;
            cachedTokens += cachedCount;
            continue;
        }
        const auto amountValue = row.value(QStringLiteral("amount"));
        const auto amount = amountValue.toObject();
        const auto currencyValue = amount.value(QStringLiteral("currency"));
        const QString currency = currencyValue.toString();
        const auto number = amount.value(QStringLiteral("value"));
        const double spent = number.toDouble(-1);
        if (rowType != QLatin1String("organization.costs.result") ||
            currency != QLatin1String("usd") || !number.isDouble() || !std::isfinite(spent) ||
            spent < 0)
            return std::nullopt;
        const auto observedAt = QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::UTC);
        const auto day = observedAt.date();
        const QString label = day.toString(Qt::ISODate);
        QVariantMap observation;
        observation.insert(QStringLiteral("label"), label);
        observation.insert(QStringLiteral("value"), spent);
        daily.append(observation);
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
    if (usage) {
        QVariantMap result;
        if (inputTokens > 0)
            result.insert(QStringLiteral("cachedInputPercent"),
                          100.0 * (cachedTokens / inputTokens));
        return result;
    }
    const QString endDate = date.toString(Qt::ISODate);
    QVariantMap cost;
    cost.insert(QStringLiteral("daily"), daily);
    cost.insert(QStringLiteral("currencyCode"), QStringLiteral("USD"));
    cost.insert(QStringLiteral("historyPartial"), true);
    cost.insert(QStringLiteral("historyEndDate"), endDate);
    cost.insert(QStringLiteral("historyIncludesCurrentDay"), true);
    if (monthReported)
        cost.insert(QStringLiteral("monthToDateUSD"), month);
    if (totalReported)
        cost.insert(QStringLiteral("last30DaysUSD"), total);
    if (today)
        cost.insert(QStringLiteral("todayUSD"), *today);
    QVariantMap result;
    result.insert(QStringLiteral("cost"), cost);
    result.insert(QStringLiteral("balanceUnavailable"), true);
    return result;
}
} // namespace

std::optional<QVariantMap> OpenAICosts::parse(const QByteArray &bytes, qint64 startTime,
                                              const QDateTime &now)
{
    return parseResponse(bytes, startTime, now, false);
}

std::optional<QVariantMap> OpenAICosts::parseUsage(const QByteArray &bytes, qint64 startTime,
                                                   const QDateTime &now)
{
    return parseResponse(bytes, startTime, now, true);
}
} // namespace Kodometer
