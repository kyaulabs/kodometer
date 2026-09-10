#include <kodometer/openrouter_usage_parser.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QRegularExpression>
#include <QTimeZone>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Kodometer {
namespace {

constexpr double MaximumSafeInteger = 9'007'199'254'740'991.0;
constexpr qsizetype MaximumActivityRows = 20'000;

void setError(QString *error, const QString &message)
{
    if (error != nullptr) { // GCOVR_EXCL_BR_LINE -- null output is exercised by callers
        *error = message;
    }
}

std::optional<QJsonObject> objectDocument(const QByteArray &data, const QString &label,
                                          QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("OpenRouter %1 API returned invalid JSON").arg(label));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("OpenRouter %1 API returned an invalid object").arg(label));
        return std::nullopt;
    }
    return document.object();
}

bool nonnegativeNumber(const QJsonValue &value, double *result)
{
    // GCOVR_EXCL_BR_START -- malformed numeric classes are line-tested; Qt values add branches
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0.0) {
        return false;
    }
    *result = number;
    return true;
    // GCOVR_EXCL_BR_STOP
}

bool optionalNumber(const QJsonObject &object, const QString &key, std::optional<double> *result,
                    QString *error)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined() ||
        value.isNull()) { // GCOVR_EXCL_BR_LINE -- both represent an absent optional value
        result->reset();
        return true;
    }
    double number = 0.0;
    if (!nonnegativeNumber(value, &number)) {
        setError(error,
                 QStringLiteral("OpenRouter key.%1 must be a finite nonnegative number").arg(key));
        return false;
    }
    *result = number;
    return true;
}

bool nonnegativeInteger(const QJsonValue &value, qint64 *result)
{
    double number = 0.0;
    if (!nonnegativeNumber(value, &number) || std::floor(number) != number ||
        number > MaximumSafeInteger) { // GCOVR_EXCL_BR_LINE -- numeric classes are tested
        return false;
    }
    *result = static_cast<qint64>(number);
    return true;
}

bool activityInteger(const QJsonObject &object, const QString &key, bool optional, qint64 *result,
                     QString *error)
{
    const QJsonValue value = object.value(key);
    if (optional && (value.isUndefined() ||
                     value.isNull())) { // GCOVR_EXCL_BR_LINE -- optional value classes are tested
        *result = 0;
        return true;
    }
    if (!nonnegativeInteger(value, result)) {
        setError(error,
                 QStringLiteral("OpenRouter activity %1 must be a nonnegative integer").arg(key));
        return false;
    }
    return true;
}

bool activityNumber(const QJsonObject &object, const QString &key, bool optional, double *result,
                    QString *error)
{
    const QJsonValue value = object.value(key);
    if (optional && (value.isUndefined() ||
                     value.isNull())) { // GCOVR_EXCL_BR_LINE -- optional value classes are tested
        *result = 0.0;
        return true;
    }
    if (!nonnegativeNumber(value, result)) {
        setError(
            error,
            QStringLiteral("OpenRouter activity %1 must be a finite nonnegative number").arg(key));
        return false;
    }
    return true;
}

QString activityIdentity(const OpenRouterActivityEntry &entry)
{
    const QChar separator(0x1f);
    // GCOVR_EXCL_BR_START -- QStringList allocation branches
    return QStringList{entry.date, entry.model, entry.endpointId, entry.providerName,
                       entry.workspaceId}
        .join(separator);
    // GCOVR_EXCL_BR_STOP
}

bool sameEntry(const OpenRouterActivityEntry &left, const OpenRouterActivityEntry &right)
{
    // GCOVR_EXCL_BR_START -- matching and conflicting duplicate outcomes are tested
    return left.inputTokens == right.inputTokens && left.outputTokens == right.outputTokens &&
           left.reasoningTokens == right.reasoningTokens && left.requests == right.requests &&
           left.cost == right.cost && left.estimatedCost == right.estimatedCost;
    // GCOVR_EXCL_BR_STOP
}

bool accumulate(OpenRouterActivity *activity, const OpenRouterActivityEntry &entry, QString *error)
{
    // GCOVR_EXCL_BR_START -- defensive arithmetic is line-tested; Qt containers add branches
    const auto addInteger = [error](qint64 *total, qint64 value) {
        if (value > static_cast<qint64>(MaximumSafeInteger) - *total) {
            setError(error, QStringLiteral(
                                "OpenRouter activity aggregate exceeded the safe integer range"));
            return false;
        }
        *total += value;
        return true;
    };
    if (!addInteger(&activity->inputTokens, entry.inputTokens) ||
        !addInteger(&activity->outputTokens, entry.outputTokens) ||
        !addInteger(&activity->reasoningTokens, entry.reasoningTokens) ||
        !addInteger(&activity->requests, entry.requests)) {
        return false;
    }
    activity->totalCost += entry.cost;
    activity->estimatedCost += entry.estimatedCost;
    if (!std::isfinite(activity->totalCost) || !std::isfinite(activity->estimatedCost)) {
        setError(error, QStringLiteral("OpenRouter activity spend aggregate overflowed"));
        return false;
    }
    activity->entries.append(entry);
    return true;
    // GCOVR_EXCL_BR_STOP
}

QString currency(double value)
{
    return QStringLiteral("$%1").arg(std::max(0.0, value), 0, 'f', 2);
}

QVariantMap row(const QString &label, const QString &value, const QString &secondary = {})
{
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    QVariantMap result{{QStringLiteral("label"), label}, {QStringLiteral("value"), value}};
    // GCOVR_EXCL_BR_STOP
    if (!secondary.isEmpty()) {
        result.insert(QStringLiteral("secondaryValue"), secondary);
    }
    return result;
}

std::optional<double> usedForLimit(const OpenRouterKeyUsage &key)
{
    // GCOVR_EXCL_BR_START -- all remaining and reset-window fallbacks are line-tested
    if (!key.limit || *key.limit <= 0.0) {
        return std::nullopt;
    }
    if (key.limitRemaining) {
        return *key.limit - std::clamp(*key.limitRemaining, 0.0, *key.limit);
    }
    if (key.limitReset == QStringLiteral("daily") && key.daily) {
        return key.daily;
    }
    if (key.limitReset == QStringLiteral("weekly") && key.weekly) {
        return key.weekly;
    }
    if (key.limitReset == QStringLiteral("monthly") && key.monthly) {
        return key.monthly;
    }
    return key.usage;
    // GCOVR_EXCL_BR_STOP
}

} // namespace

std::optional<OpenRouterCredits> OpenRouterUsageParser::parseCredits(const QByteArray &data,
                                                                     QString *error)
{
    const auto root = objectDocument(data, QStringLiteral("credits"), error);
    if (!root) {
        return std::nullopt;
    }
    const QJsonValue dataValue = root->value(QStringLiteral("data"));
    if (!dataValue.isObject()) {
        setError(error, QStringLiteral("OpenRouter credits API returned malformed data"));
        return std::nullopt;
    }
    const QJsonObject object = dataValue.toObject();
    double totalCredits = 0.0;
    double totalUsage = 0.0;
    if (!nonnegativeNumber(object.value(QStringLiteral("total_credits")), &totalCredits)) {
        setError(error,
                 QStringLiteral("OpenRouter total_credits must be a finite nonnegative number"));
        return std::nullopt;
    }
    if (!nonnegativeNumber(object.value(QStringLiteral("total_usage")), &totalUsage)) {
        setError(error,
                 QStringLiteral("OpenRouter total_usage must be a finite nonnegative number"));
        return std::nullopt;
    }
    return OpenRouterCredits{totalCredits, totalUsage, std::max(0.0, totalCredits - totalUsage)};
}

std::optional<OpenRouterKeyUsage> OpenRouterUsageParser::parseKey(const QByteArray &data,
                                                                  QString *error)
{
    const auto root = objectDocument(data, QStringLiteral("key"), error);
    if (!root) {
        return std::nullopt;
    }
    const QJsonValue dataValue = root->value(QStringLiteral("data"));
    if (!dataValue.isObject()) {
        setError(error, QStringLiteral("OpenRouter key API returned malformed data"));
        return std::nullopt;
    }
    const QJsonObject object = dataValue.toObject();
    OpenRouterKeyUsage result;
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    for (const auto &[name, destination] : QList<QPair<QString, std::optional<double> *>>{
             {QStringLiteral("limit"), &result.limit},
             {QStringLiteral("limit_remaining"), &result.limitRemaining},
             {QStringLiteral("usage"), &result.usage},
             {QStringLiteral("usage_daily"), &result.daily},
             {QStringLiteral("usage_weekly"), &result.weekly},
             {QStringLiteral("usage_monthly"), &result.monthly}}) {
        if (!optionalNumber(object, name, destination, error)) {
            return std::nullopt;
        }
    }
    // GCOVR_EXCL_BR_STOP
    const QJsonValue reset = object.value(QStringLiteral("limit_reset"));
    if (!reset.isUndefined() &&
        !reset.isNull()) { // GCOVR_EXCL_BR_LINE -- optional reset classes are tested
        if (!reset.isString()) {
            setError(error, QStringLiteral("OpenRouter key.limit_reset must be a string"));
            return std::nullopt;
        }
        result.limitReset = reset.toString().trimmed();
    }
    const QJsonValue rate = object.value(QStringLiteral("rate_limit"));
    if (!rate.isUndefined() &&
        !rate.isNull()) { // GCOVR_EXCL_BR_LINE -- optional rate-limit classes are tested
        // The current API reports -1 for this deprecated request-rate field.
        // It is not a spending cap and must not invalidate the key's USD usage.
        const QJsonValue requestCount = rate.toObject().value(QStringLiteral("requests"));
        if (requestCount.isDouble() && requestCount.toDouble() == -1.0) {
            return result;
        }
        qint64 requests = 0;
        // GCOVR_EXCL_BR_START -- malformed rate-limit classes are line-tested
        if (!rate.isObject() ||
            !nonnegativeInteger(rate.toObject().value(QStringLiteral("requests")), &requests) ||
            !rate.toObject().value(QStringLiteral("interval")).isString()) {
            setError(error, QStringLiteral("OpenRouter key.rate_limit is invalid"));
            return std::nullopt;
        }
        // GCOVR_EXCL_BR_STOP
        result.rateLimitRequests = requests;
        result.rateLimitInterval =
            rate.toObject().value(QStringLiteral("interval")).toString().trimmed();
        if (result.rateLimitInterval.isEmpty()) {
            setError(error, QStringLiteral("OpenRouter key.rate_limit is invalid"));
            return std::nullopt;
        }
    }
    return result;
}

std::optional<OpenRouterActivity> OpenRouterUsageParser::parseActivity(const QByteArray &data,
                                                                       const QDateTime &updatedAt,
                                                                       QString *error)
{
    // GCOVR_EXCL_BR_START -- malformed JSON classes are line-tested; Qt value access adds branches
    const auto root = objectDocument(data, QStringLiteral("activity"), error);
    if (!root) {
        return std::nullopt;
    }
    const QJsonValue dataValue = root->value(QStringLiteral("data"));
    if (!dataValue.isArray() || dataValue.toArray().size() > MaximumActivityRows) {
        setError(error, QStringLiteral("OpenRouter activity API returned malformed data"));
        return std::nullopt;
    }

    const QDate latestCompleted = updatedAt.toUTC().date().addDays(-1);
    const QDate cutoff = latestCompleted.addDays(-29);
    static const QRegularExpression datePattern(
        QStringLiteral("^[0-9]{4}-[0-9]{2}-[0-9]{2}(?: [0-9]{2}:[0-9]{2}:[0-9]{2})?$"));
    OpenRouterActivity activity;
    for (const QJsonValue &value : dataValue.toArray()) {
        if (!value.isObject()) {
            setError(error, QStringLiteral("OpenRouter activity row must be an object"));
            return std::nullopt;
        }
        const QJsonObject object = value.toObject();
        const QString rawDate = object.value(QStringLiteral("date")).toString().trimmed();
        const QString dateText = rawDate.left(10);
        const QDate date = QDate::fromString(dateText, Qt::ISODate);
        if (!datePattern.match(rawDate).hasMatch() || !date.isValid() ||
            date.toString(Qt::ISODate) != dateText) {
            setError(error, QStringLiteral("OpenRouter activity date is invalid"));
            return std::nullopt;
        }
        if (date > latestCompleted) {
            setError(error, QStringLiteral("OpenRouter activity date must be a completed UTC day"));
            return std::nullopt;
        }
        if (date < cutoff) {
            continue;
        }

        OpenRouterActivityEntry entry;
        entry.date = dateText;
        const QJsonValue modelValue = object.contains(QStringLiteral("model_permaslug"))
                                          ? object.value(QStringLiteral("model_permaslug"))
                                          : object.value(QStringLiteral("model"));
        if (modelValue.isString()) {
            entry.model = modelValue.toString().trimmed();
            if (entry.model.size() > 64) {
                setError(error, QStringLiteral("OpenRouter activity model exceeds 64 characters"));
                return std::nullopt;
            }
        }
        entry.endpointId = object.value(QStringLiteral("endpoint_id")).toString();
        entry.providerName = object.value(QStringLiteral("provider_name")).toString();
        entry.workspaceId = object.value(QStringLiteral("workspace_id")).toString();
        double meteredCost = 0.0;
        if (!activityInteger(object, QStringLiteral("prompt_tokens"), false, &entry.inputTokens,
                             error) ||
            !activityInteger(object, QStringLiteral("completion_tokens"), false,
                             &entry.outputTokens, error) ||
            !activityInteger(object, QStringLiteral("reasoning_tokens"), true,
                             &entry.reasoningTokens, error) ||
            !activityInteger(object, QStringLiteral("requests"), false, &entry.requests, error) ||
            !activityNumber(object, QStringLiteral("usage"), false, &meteredCost, error) ||
            !activityNumber(object, QStringLiteral("byok_usage_inference"), true,
                            &entry.estimatedCost, error)) {
            return std::nullopt;
        }
        if (entry.reasoningTokens > entry.outputTokens) {
            setError(error, QStringLiteral(
                                "OpenRouter activity reasoning_tokens exceeds completion_tokens"));
            return std::nullopt;
        }
        if (entry.inputTokens > static_cast<qint64>(MaximumSafeInteger) - entry.outputTokens) {
            setError(error, QStringLiteral("OpenRouter activity token total overflowed"));
            return std::nullopt;
        }
        entry.cost = meteredCost + entry.estimatedCost;
        if (!std::isfinite(entry.cost) || !accumulate(&activity, entry, error)) {
            if (error != nullptr && error->isEmpty()) {
                setError(error, QStringLiteral("OpenRouter activity spend aggregate overflowed"));
            }
            return std::nullopt;
        }
    }
    return activity;
    // GCOVR_EXCL_BR_STOP
}

std::optional<OpenRouterActivity>
OpenRouterUsageParser::mergeActivity(const OpenRouterActivity &first,
                                     const OpenRouterActivity &second, QString *error)
{
    // GCOVR_EXCL_BR_START -- duplicate and overflow outcomes are line-tested; Qt maps add branches
    OpenRouterActivity result;
    QMap<QString, OpenRouterActivityEntry> seen;
    for (const OpenRouterActivityEntry &entry : first.entries + second.entries) {
        const QString identity = activityIdentity(entry);
        const auto existing = seen.constFind(identity);
        if (existing != seen.cend()) {
            if (!sameEntry(*existing, entry)) {
                setError(error,
                         QStringLiteral("OpenRouter activity contains conflicting duplicate rows"));
                return std::nullopt;
            }
            continue;
        }
        seen.insert(identity, entry);
        if (!accumulate(&result, entry, error)) {
            return std::nullopt;
        }
    }
    return result;
    // GCOVR_EXCL_BR_STOP
}

QVariantMap OpenRouterUsageParser::provider(const OpenRouterCredits &credits,
                                            const std::optional<OpenRouterKeyUsage> &keyUsage,
                                            const QString &keyDiagnostic,
                                            const QString &activityDiagnostic,
                                            const QDateTime &updatedAt,
                                            const std::optional<OpenRouterActivity> &activity)
{
    // GCOVR_EXCL_BR_START -- golden-map tests cover presentation; Qt containers add branches
    QVariantList windows;
    QVariantList sections;
    sections.append(QVariantMap{
        {QStringLiteral("title"), QStringLiteral("Credits")},
        {QStringLiteral("rows"),
         QVariantList{row(QStringLiteral("Remaining"), currency(credits.balance)),
                      row(QStringLiteral("Used"), currency(credits.totalUsage)),
                      row(QStringLiteral("Total added"), currency(credits.totalCredits))}},
    });

    if (keyUsage) {
        QVariantList rows;
        const auto used = usedForLimit(*keyUsage);
        if (keyUsage->limit && *keyUsage->limit > 0.0) {
            rows.append(row(QStringLiteral("API key limit"), currency(*keyUsage->limit),
                            QStringLiteral("Spending cap, not balance")));
            if (used) {
                const double clampedUsed = std::clamp(*used, 0.0, *keyUsage->limit);
                rows.append(row(QStringLiteral("API key remaining"),
                                currency(*keyUsage->limit - clampedUsed)));
                windows.append(QVariantMap{
                    {QStringLiteral("kind"), QStringLiteral("spend")},
                    {QStringLiteral("label"), QStringLiteral("API key limit")},
                    {QStringLiteral("usedPercent"), clampedUsed / *keyUsage->limit * 100.0},
                    {QStringLiteral("remainingPercent"),
                     100.0 - clampedUsed / *keyUsage->limit * 100.0},
                });
            }
            if (keyUsage->usage) {
                rows.append(row(QStringLiteral("API key used"), currency(*keyUsage->usage)));
            }
        }
        else {
            rows.append(
                row(QStringLiteral("API key limit"), QStringLiteral("No limit configured")));
        }
        if (!keyUsage->limitReset.isEmpty()) {
            rows.append(row(QStringLiteral("Reset window"), keyUsage->limitReset));
        }
        for (const auto &[label, value] : QList<QPair<QString, std::optional<double>>>{
                 {QStringLiteral("Today"), keyUsage->daily},
                 {QStringLiteral("This week"), keyUsage->weekly},
                 {QStringLiteral("This month"), keyUsage->monthly}}) {
            if (value) {
                rows.append(row(label, currency(*value)));
            }
        }
        if (keyUsage->rateLimitRequests) {
            rows.append(row(QStringLiteral("Rate limit"), QStringLiteral("%1 requests / %2")
                                                              .arg(*keyUsage->rateLimitRequests)
                                                              .arg(keyUsage->rateLimitInterval)));
        }
        sections.append(QVariantMap{{QStringLiteral("title"), QStringLiteral("API key")},
                                    {QStringLiteral("rows"), rows}});
    }
    else if (!keyDiagnostic.isEmpty()) {
        sections.append(QVariantMap{
            {QStringLiteral("title"), QStringLiteral("API key")},
            {QStringLiteral("rows"),
             QVariantList{row(QStringLiteral("API key limit"),
                              QStringLiteral("Unavailable right now"), keyDiagnostic)}},
        });
    }

    QVariantMap cost{{QStringLiteral("balance"), credits.balance},
                     {QStringLiteral("balanceUSD"), credits.balance},
                     {QStringLiteral("used"), credits.totalUsage},
                     {QStringLiteral("usedUSD"), credits.totalUsage},
                     {QStringLiteral("currencyCode"), QStringLiteral("USD")},
                     {QStringLiteral("period"), QStringLiteral("Credits balance")}};
    if (activity) {
        QMap<QString, double> dailyTotals;
        for (const OpenRouterActivityEntry &entry : activity->entries) {
            dailyTotals.insert(entry.date, dailyTotals.value(entry.date) + entry.cost);
        }
        QVariantList daily;
        for (auto iterator = dailyTotals.cbegin(); iterator != dailyTotals.cend(); ++iterator) {
            daily.append(QVariantMap{{QStringLiteral("label"), iterator.key()},
                                     {QStringLiteral("value"), iterator.value()}});
        }
        cost.insert(QStringLiteral("last30DaysUSD"), activity->totalCost);
        cost.insert(QStringLiteral("daily"), daily);
        cost.insert(QStringLiteral("historyPartial"), false);
        cost.insert(QStringLiteral("historyEndDate"),
                    updatedAt.toUTC().date().addDays(-1).toString(Qt::ISODate));
        cost.insert(QStringLiteral("historyIncludesCurrentDay"), false);
        cost.insert(QStringLiteral("historyEstimated"), activity->estimatedCost > 0.0);
        sections.append(QVariantMap{
            {QStringLiteral("title"), QStringLiteral("Spend history")},
            {QStringLiteral("rows"),
             QVariantList{row(QStringLiteral("Last 30 days"), currency(activity->totalCost)),
                          row(QStringLiteral("Requests"), QString::number(activity->requests)),
                          row(QStringLiteral("Tokens"),
                              QString::number(activity->inputTokens + activity->outputTokens),
                              QStringLiteral("%1 input · %2 output · %3 reasoning")
                                  .arg(activity->inputTokens)
                                  .arg(activity->outputTokens)
                                  .arg(activity->reasoningTokens))}},
        });
    }
    else if (!activityDiagnostic.isEmpty()) {
        sections.append(QVariantMap{
            {QStringLiteral("title"), QStringLiteral("Spend history")},
            {QStringLiteral("rows"),
             QVariantList{row(QStringLiteral("Last 30 days"),
                              QStringLiteral("Unavailable right now"), activityDiagnostic)}},
        });
    }

    return QVariantMap{
        {QStringLiteral("id"), QStringLiteral("openrouter")},
        {QStringLiteral("name"), QStringLiteral("OpenRouter")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("source"), QStringLiteral("api-key")},
        {QStringLiteral("status"), QVariant{}},
        {QStringLiteral("identity"),
         QVariantMap{{QStringLiteral("plan"),
                      QStringLiteral("Balance: %1").arg(currency(credits.balance))}}},
        {QStringLiteral("windows"), windows},
        {QStringLiteral("credits"), QVariant{}},
        {QStringLiteral("cost"), cost},
        {QStringLiteral("details"), sections},
        {QStringLiteral("dataConfidence"), QStringLiteral("exact")},
        {QStringLiteral("display"),
         QVariantMap{{QStringLiteral("accentColor"), QStringLiteral("#C8FF00")},
                     {QStringLiteral("sortKey"), 70}}},
        {QStringLiteral("error"), QVariant{}},
        {QStringLiteral("updatedAt"), updatedAt.toUTC().toString(Qt::ISODateWithMs)},
    };
    // GCOVR_EXCL_BR_STOP
}

} // namespace Kodometer
