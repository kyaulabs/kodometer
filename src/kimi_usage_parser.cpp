#include <kodometer/kimi_usage_parser.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimeZone>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Kodometer {
namespace {

struct UsageDetail
{
    double limit = 0.0;
    double used = 0.0;
    bool reliable = false;
    QString resetAt;
};

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

std::optional<double> number(const QJsonValue &value)
{
    double result = 0.0;
    bool valid = false;
    if (value.isDouble()) {
        result = value.toDouble();
        valid = true;
    }
    else if (value.isString()) {
        result = value.toString().trimmed().toDouble(&valid);
    }
    if (!valid ||
        !std::isfinite(result)) { // GCOVR_EXCL_BR_LINE -- numeric input classes are tested
        return std::nullopt;
    }
    return result;
}

QString resetTimestamp(const QJsonObject &object)
{
    QString raw;
    // GCOVR_EXCL_BR_START -- reset aliases are tested; Qt iteration adds branches
    for (const QString &key : {QStringLiteral("resetTime"), QStringLiteral("resetAt"),
                               QStringLiteral("reset_time"), QStringLiteral("reset_at")}) {
        const QJsonValue value = object.value(key);
        if (value.isString() && !value.toString().trimmed().isEmpty()) {
            raw = value.toString().trimmed();
            break;
        }
    }
    // GCOVR_EXCL_BR_STOP
    if (raw.isEmpty()) {
        return {};
    }
    const QDateTime parsed = QDateTime::fromString(raw, Qt::ISODate);
    // GCOVR_EXCL_BR_START -- valid and invalid Qt timestamps are tested
    const QString timestamp =
        parsed.isValid() ? parsed.toUTC().toString(Qt::ISODateWithMs) : QString{};
    // GCOVR_EXCL_BR_STOP
    return timestamp;
}

std::optional<UsageDetail> detail(const QJsonValue &value)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const auto limit = number(object.value(QStringLiteral("limit")));
    if (!limit || *limit <= 0.0) { // GCOVR_EXCL_BR_LINE -- both failures are tested
        return std::nullopt;
    }

    UsageDetail result;
    result.limit = *limit;
    result.resetAt = resetTimestamp(object);
    const auto used = number(object.value(QStringLiteral("used")));
    if (used && *used >= 0.0) {
        result.used = *used;
        result.reliable = true;
        return result;
    }
    const auto remaining = number(object.value(QStringLiteral("remaining")));
    if (remaining && *remaining >= 0.0 &&
        *remaining <= *limit) { // GCOVR_EXCL_BR_LINE -- fallback boundaries are tested
        result.used = *limit - *remaining;
        result.reliable = true;
    }
    return result;
}

std::optional<int> windowSeconds(const QJsonObject &window)
{
    const auto duration = number(window.value(QStringLiteral("duration")));
    if (!duration || *duration <= 0.0 ||
        std::floor(*duration) != *duration) { // GCOVR_EXCL_BR_LINE -- invalid forms are tested
        return std::nullopt;
    }
    qint64 multiplier = 0;
    const QString unit = window.value(QStringLiteral("timeUnit")).toString();
    if (unit == QStringLiteral("TIME_UNIT_MINUTE")) {
        multiplier = 60;
    }
    else if (unit == QStringLiteral("TIME_UNIT_HOUR")) {
        multiplier = 60 * 60;
    }
    else if (unit == QStringLiteral("TIME_UNIT_DAY")) {
        multiplier = 24 * 60 * 60;
    }
    else {
        return std::nullopt;
    }
    if (*duration >
        static_cast<double>(std::numeric_limits<int>::max()) / static_cast<double>(multiplier)) {
        return std::nullopt;
    }
    return static_cast<int>(*duration) * static_cast<int>(multiplier);
}

QString rateLabel(const std::optional<int> &seconds)
{
    if (!seconds) {
        return QStringLiteral("Rate limit");
    }
    if (*seconds % (60 * 60) == 0) {
        return QStringLiteral("%1-hour usage").arg(*seconds / (60 * 60));
    }
    if (*seconds % 60 == 0) { // GCOVR_EXCL_BR_LINE -- supported units are minute-aligned
        return QStringLiteral("%1-minute usage").arg(*seconds / 60);
    }
    return QStringLiteral("Rate limit");
}

QVariantMap mappedWindow(const UsageDetail &usage, const QString &kind, const QString &label,
                         const std::optional<int> &seconds)
{
    const double usedPercent = std::clamp(usage.used / usage.limit * 100.0, 0.0, 100.0);
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    QVariantMap window{{QStringLiteral("kind"), kind},
                       {QStringLiteral("label"), label},
                       {QStringLiteral("usedPercent"), usedPercent},
                       {QStringLiteral("remainingPercent"), 100.0 - usedPercent}};
    // GCOVR_EXCL_BR_STOP
    if (usage.reliable && seconds) {
        window.insert(QStringLiteral("windowSeconds"), *seconds);
    }
    if (!usage.resetAt.isEmpty()) {
        window.insert(QStringLiteral("resetAt"), usage.resetAt);
    }
    return window;
}

QString planFor(double weeklyLimit)
{
    if (weeklyLimit == 1024.0) {
        return QStringLiteral("Andante");
    }
    if (weeklyLimit == 2048.0) {
        return QStringLiteral("Moderato");
    }
    if (weeklyLimit == 7168.0) {
        return QStringLiteral("Allegretto");
    }
    return {};
}

} // namespace

std::optional<QVariantMap> KimiUsageParser::parse(const QByteArray &data,
                                                  KimiCredentialSource source,
                                                  const QDateTime &updatedAt, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Kimi Code usage API returned invalid JSON"));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("Kimi Code usage API returned an invalid object"));
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    const QJsonValue usageValue = root.value(QStringLiteral("usage"));
    if (!usageValue.isObject()) {
        setError(error, QStringLiteral("Kimi Code usage API returned no usage detail"));
        return std::nullopt;
    }
    const auto weekly = detail(usageValue);
    if (!weekly) {
        setError(error, QStringLiteral("Kimi Code usage API returned an invalid weekly limit"));
        return std::nullopt;
    }

    QVariantList windows;
    windows.append(mappedWindow(*weekly, QStringLiteral("weekly"), QStringLiteral("7-day usage"),
                                7 * 24 * 60 * 60));
    const QJsonArray limits = root.value(QStringLiteral("limits")).toArray();
    for (const QJsonValue &limitValue : limits) {
        if (!limitValue.isObject()) {
            continue;
        }
        const QJsonObject limit = limitValue.toObject();
        const auto rate = detail(limit.value(QStringLiteral("detail")));
        if (!rate) {
            continue;
        }
        const auto duration = windowSeconds(limit.value(QStringLiteral("window")).toObject());
        windows.append(
            mappedWindow(*rate, QStringLiteral("session"), rateLabel(duration), duration));
        break;
    }

    QVariantMap identity;
    const QString plan = planFor(weekly->limit);
    if (!plan.isEmpty()) {
        identity.insert(QStringLiteral("plan"), plan);
    }
    const QString sourceName = source == KimiCredentialSource::ApiKey ? QStringLiteral("api-key")
                                                                      : QStringLiteral("cli-oauth");

    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    const QVariantMap provider{
        {QStringLiteral("id"), QStringLiteral("kimi")},
        {QStringLiteral("name"), QStringLiteral("Kimi Code")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("source"), sourceName},
        {QStringLiteral("status"), QVariant{}},
        {QStringLiteral("identity"), identity},
        {QStringLiteral("windows"), windows},
        {QStringLiteral("credits"), QVariant{}},
        {QStringLiteral("cost"), QVariant{}},
        {QStringLiteral("display"),
         QVariantMap{{QStringLiteral("accentColor"), QStringLiteral("#FE603C")},
                     {QStringLiteral("sortKey"), 40}}},
        {QStringLiteral("error"), QVariant{}},
        {QStringLiteral("updatedAt"), updatedAt.toUTC().toString(Qt::ISODateWithMs)},
    };
    // GCOVR_EXCL_BR_STOP
    return provider;
}

} // namespace Kodometer
