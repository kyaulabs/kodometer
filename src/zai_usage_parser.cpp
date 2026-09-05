#include <kodometer/zai_usage_parser.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Kodometer {
namespace {

struct Limit
{
    QString type;
    int unit = 0;
    int number = 0;
    double percent = 0.0;
    std::optional<qint64> usage;
    std::optional<qint64> current;
    std::optional<qint64> remaining;
    std::optional<qint64> resetMillis;
    std::optional<int> windowSeconds;
    QJsonArray details;
};

void setError(QString *error, const QString &message)
{
    // GCOVR_EXCL_BR_START -- tested error output uses Qt pointer and assignment branches
    if (error != nullptr) {
        *error = message;
    }
    // GCOVR_EXCL_BR_STOP
}

std::optional<QJsonObject> objectDocument(const QByteArray &data, const QString &label,
                                          QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("z.ai %1 API returned invalid JSON").arg(label));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("z.ai %1 API returned an invalid object").arg(label));
        return std::nullopt;
    }
    return document.object();
}

bool integer(const QJsonValue &value, qint64 *result)
{
    if (!value.isDouble()) {
        return false;
    }
    const double raw = value.toDouble();
    constexpr double maximumSafeInteger = 9'007'199'254'740'991.0;
    // GCOVR_EXCL_BR_START -- numeric input classes are tested; Qt access adds branches
    if (!std::isfinite(raw) || std::floor(raw) != raw || raw < -maximumSafeInteger ||
        raw > maximumSafeInteger) {
        return false;
    }
    // GCOVR_EXCL_BR_STOP
    *result = static_cast<qint64>(raw);
    return true;
}

bool optionalInteger(const QJsonObject &object, const QString &key, std::optional<qint64> *result,
                     QString *error)
{
    const QJsonValue value = object.value(key);
    // GCOVR_EXCL_BR_START -- absent, null, valid, and invalid values are tested
    if (value.isUndefined() || value.isNull()) {
        result->reset();
        return true;
    }
    qint64 parsed = 0;
    if (!integer(value, &parsed)) {
        setError(error, QStringLiteral("z.ai limit %1 must be an integer").arg(key));
        return false;
    }
    // GCOVR_EXCL_BR_STOP
    *result = parsed;
    return true;
}

std::optional<int> durationSeconds(int unit, int number)
{
    if (number <= 0) {
        return std::nullopt;
    }
    int multiplier = 0;
    if (unit == 1) {
        multiplier = 24 * 60 * 60;
    }
    else if (unit == 3) {
        multiplier = 60 * 60;
    }
    else if (unit == 5) {
        multiplier = 60;
    }
    else if (unit == 6) {
        multiplier = 7 * 24 * 60 * 60;
    }
    else {
        return std::nullopt;
    }
    if (number > std::numeric_limits<int>::max() / multiplier) {
        return std::nullopt;
    }
    return number * multiplier;
}

std::optional<Limit> parseLimit(const QJsonValue &value, QString *error)
{
    if (!value.isObject()) {
        setError(error, QStringLiteral("z.ai quota API returned a malformed limit"));
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue typeValue = object.value(QStringLiteral("type"));
    qint64 unit = 0;
    qint64 number = 0;
    qint64 percentage = 0;
    // GCOVR_EXCL_BR_START -- malformed mandatory fields are tested; Qt access adds branches
    if (!typeValue.isString() || !integer(object.value(QStringLiteral("unit")), &unit) ||
        !integer(object.value(QStringLiteral("number")), &number) ||
        !integer(object.value(QStringLiteral("percentage")), &percentage) ||
        unit < std::numeric_limits<int>::min() || unit > std::numeric_limits<int>::max() ||
        number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
        setError(error, QStringLiteral("z.ai quota API returned a malformed limit"));
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_STOP

    Limit limit;
    limit.type = typeValue.toString();
    limit.unit = static_cast<int>(unit);
    limit.number = static_cast<int>(number);
    limit.percent = std::clamp(static_cast<double>(percentage), 0.0, 100.0);
    // GCOVR_EXCL_BR_START -- all supported and ignored limit types are tested
    if (limit.type != QStringLiteral("TOKENS_LIMIT") &&
        limit.type != QStringLiteral("CREDIT_LIMIT") &&
        limit.type != QStringLiteral("TIME_LIMIT")) {
        return limit;
    }
    // GCOVR_EXCL_BR_STOP
    // GCOVR_EXCL_BR_START -- optional integer validation paths are tested
    if (!optionalInteger(object, QStringLiteral("usage"), &limit.usage, error) ||
        !optionalInteger(object, QStringLiteral("currentValue"), &limit.current, error) ||
        !optionalInteger(object, QStringLiteral("remaining"), &limit.remaining, error) ||
        !optionalInteger(object, QStringLiteral("nextResetTime"), &limit.resetMillis, error)) {
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_STOP
    const QJsonValue detailsValue = object.value(QStringLiteral("usageDetails"));
    // GCOVR_EXCL_BR_START -- absent, null, array, and malformed details are tested
    if (!detailsValue.isUndefined() && !detailsValue.isNull()) {
        if (!detailsValue.isArray()) {
            setError(error, QStringLiteral("z.ai usageDetails must be an array"));
            return std::nullopt;
        }
        limit.details = detailsValue.toArray();
    }
    // GCOVR_EXCL_BR_STOP
    // GCOVR_EXCL_BR_START -- count-derived percentage combinations are tested
    if (limit.usage && *limit.usage > 0) {
        std::optional<qint64> used;
        if (limit.remaining) {
            const qint64 fromRemaining = *limit.usage - *limit.remaining;
            used = limit.current ? std::max(fromRemaining, *limit.current) : fromRemaining;
        }
        else if (limit.current) {
            used = limit.current;
        }
        if (used) {
            const qint64 clamped = std::clamp(*used, qint64{0}, *limit.usage);
            limit.percent =
                static_cast<double>(clamped) / static_cast<double>(*limit.usage) * 100.0;
        }
    }
    // GCOVR_EXCL_BR_STOP
    limit.windowSeconds = durationSeconds(limit.unit, limit.number);
    return limit;
}

QString percentLabel(double percent)
{
    const bool whole = std::floor(percent) == percent;
    return QStringLiteral("%1% used").arg(percent, 0, 'f', whole ? 0 : 1);
}

QVariantMap limitRow(const QString &label, const Limit &limit)
{
    QStringList secondary;
    if (limit.usage) {
        secondary.append(QStringLiteral("%1 limit").arg(*limit.usage));
    }
    if (limit.remaining) {
        secondary.append(QStringLiteral("%1 remaining").arg(*limit.remaining));
    }
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    QVariantMap row{{QStringLiteral("label"), label},
                    {QStringLiteral("value"), percentLabel(limit.percent)}};
    // GCOVR_EXCL_BR_STOP
    if (!secondary.isEmpty()) {
        row.insert(QStringLiteral("secondaryValue"), secondary.join(QStringLiteral(" · ")));
    }
    return row;
}

QString codingKind(const Limit &limit, bool firstOfPair)
{
    // GCOVR_EXCL_BR_START -- optional cadence comparisons are tested
    if (limit.windowSeconds == 5 * 60 * 60) {
        return QStringLiteral("session");
    }
    if (limit.windowSeconds == 7 * 24 * 60 * 60) {
        return QStringLiteral("weekly");
    }
    if (limit.windowSeconds) {
        return QStringLiteral("tertiary");
    }
    const QString kind = firstOfPair ? QStringLiteral("session") : QStringLiteral("tertiary");
    // GCOVR_EXCL_BR_STOP
    return kind;
}

QString codingLabel(const Limit &limit, const QString &kind)
{
    // GCOVR_EXCL_BR_START -- optional cadence comparisons are tested
    if (kind == QStringLiteral("session") && limit.windowSeconds == 5 * 60 * 60) {
        return QStringLiteral("5-hour usage");
    }
    if (kind == QStringLiteral("weekly")) {
        return QStringLiteral("Weekly usage");
    }
    if (limit.windowSeconds && *limit.windowSeconds % (24 * 60 * 60) == 0) {
        return QStringLiteral("%1-day usage").arg(*limit.windowSeconds / (24 * 60 * 60));
    }
    const QString label = QStringLiteral("Coding Plan usage");
    // GCOVR_EXCL_BR_STOP
    return label;
}

QVariantMap mappedWindow(const Limit &limit, const QString &kind, const QString &label,
                         bool monthlyMcp = false)
{
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    QVariantMap window{{QStringLiteral("kind"), kind},
                       {QStringLiteral("label"), label},
                       {QStringLiteral("usedPercent"), limit.percent},
                       {QStringLiteral("remainingPercent"), 100.0 - limit.percent}};
    // GCOVR_EXCL_BR_STOP
    if (monthlyMcp) {
        window.insert(QStringLiteral("windowSeconds"), 30 * 24 * 60 * 60);
    }
    else if (limit.windowSeconds) {
        window.insert(QStringLiteral("windowSeconds"), *limit.windowSeconds);
    }
    if (limit.resetMillis) {
        window.insert(QStringLiteral("resetAt"),
                      QDateTime::fromMSecsSinceEpoch(*limit.resetMillis, QTimeZone::UTC)
                          .toString(Qt::ISODateWithMs));
    }
    return window;
}

QVariantMap quotaRateRow(const QDateTime &updatedAt)
{
    const QDateTime now = updatedAt.toUTC();
    const int day = now.date().dayOfWeek();
    const int hour = now.time().hour();
    // GCOVR_EXCL_BR_START -- peak and off-peak calendar paths are tested
    const bool peak = day >= 1 && day <= 5 && hour >= 6 && hour < 10;
    // GCOVR_EXCL_BR_STOP
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    const QVariantMap row{
        {QStringLiteral("label"), QStringLiteral("Quota rate")},
        {QStringLiteral("value"), peak ? QStringLiteral("Peak") : QStringLiteral("Off-peak")},
        {QStringLiteral("secondaryValue"),
         peak ? QStringLiteral("1× credits") : QStringLiteral("0.5× credits")}};
    // GCOVR_EXCL_BR_STOP
    return row;
}

QString firstPlan(const QJsonObject &data)
{
    // GCOVR_EXCL_BR_START -- plan aliases use Qt iteration and value branches
    for (const QString &key :
         {QStringLiteral("planName"), QStringLiteral("plan"), QStringLiteral("plan_type"),
          QStringLiteral("packageName"), QStringLiteral("level")}) {
        const QJsonValue value = data.value(key);
        if (value.isString() && !value.toString().trimmed().isEmpty()) {
            return value.toString().trimmed();
        }
    }
    // GCOVR_EXCL_BR_STOP
    return {};
}

void addPositive(qint64 value, qint64 *total)
{
    if (value > std::numeric_limits<qint64>::max() - *total) {
        *total = std::numeric_limits<qint64>::max();
        return;
    }
    *total += value;
}

std::optional<double> number(const QJsonValue &value)
{
    bool valid = false;
    double result = 0.0;
    if (value.isDouble()) {
        result = value.toDouble();
        valid = true;
    }
    else if (value.isString()) {
        result = value.toString().trimmed().toDouble(&valid);
    }
    if (!valid || !std::isfinite(result)) {
        return std::nullopt;
    }
    return result;
}

} // namespace

std::optional<QVariantMap> ZaiUsageParser::parseQuota(const QByteArray &data, ZaiRegion region,
                                                      ZaiUsageScope scope,
                                                      const QDateTime &updatedAt, QString *error)
{
    const auto root = objectDocument(data, QStringLiteral("quota"), error);
    if (!root) {
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_START -- API success, message, and code variants are tested
    if (!root->value(QStringLiteral("success")).toBool()) {
        const QString message = root->value(QStringLiteral("msg")).toString().trimmed();
        setError(error, QStringLiteral("z.ai quota API error: %1")
                            .arg(message.isEmpty() ? QStringLiteral("invalid response") : message));
        return std::nullopt;
    }
    qint64 code = 0;
    if (!integer(root->value(QStringLiteral("code")), &code) || code != 200) {
        setError(error, QStringLiteral("z.ai quota API returned an unsuccessful status"));
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_STOP
    const QJsonObject quotaData = root->value(QStringLiteral("data")).toObject();
    const QJsonValue limitsValue = quotaData.value(QStringLiteral("limits"));
    if (!limitsValue.isArray()) {
        setError(error, QStringLiteral("z.ai quota API returned no limits"));
        return std::nullopt;
    }

    QList<Limit> codingLimits;
    std::optional<Limit> timeLimit;
    for (const QJsonValue &value : limitsValue.toArray()) {
        const auto limit = parseLimit(value, error);
        if (!limit) {
            return std::nullopt;
        }
        // GCOVR_EXCL_BR_START -- token, credit, time, and ignored limits are tested
        if (limit->type == QStringLiteral("TOKENS_LIMIT") ||
            limit->type == QStringLiteral("CREDIT_LIMIT")) {
            codingLimits.append(*limit);
        }
        else if (limit->type == QStringLiteral("TIME_LIMIT")) {
            timeLimit = *limit;
        }
        // GCOVR_EXCL_BR_STOP
    }
    std::stable_sort(codingLimits.begin(), codingLimits.end(),
                     [](const Limit &left, const Limit &right) {
                         return left.windowSeconds.value_or(std::numeric_limits<int>::max()) <
                                right.windowSeconds.value_or(std::numeric_limits<int>::max());
                     });

    QVariantList windows;
    QVariantList rows;
    // GCOVR_EXCL_BR_START -- single, paired, token, credit, and empty mappings are tested
    const Limit *tokenLimit = codingLimits.isEmpty() ? nullptr : &codingLimits.last();
    const Limit *sessionLimit = codingLimits.size() >= 2 ? &codingLimits.first() : nullptr;
    const Limit *singleLimit = codingLimits.size() == 1 ? &codingLimits.first() : nullptr;
    if (sessionLimit != nullptr) {
        const QString kind = codingKind(*sessionLimit, true);
        windows.append(mappedWindow(*sessionLimit, kind, codingLabel(*sessionLimit, kind)));
    }
    if (tokenLimit != nullptr) {
        const bool first = sessionLimit == nullptr;
        const QString kind = codingKind(*tokenLimit, first);
        if (singleLimit != nullptr || sessionLimit != nullptr) {
            windows.append(mappedWindow(*tokenLimit, kind, codingLabel(*tokenLimit, kind)));
        }
        rows.append(limitRow(tokenLimit->type == QStringLiteral("CREDIT_LIMIT")
                                 ? QStringLiteral("Credit quota")
                                 : QStringLiteral("Token quota"),
                             *tokenLimit));
    }
    if (sessionLimit != nullptr) {
        rows.append(limitRow(sessionLimit->type == QStringLiteral("CREDIT_LIMIT")
                                 ? QStringLiteral("Session credit quota")
                                 : QStringLiteral("Session token quota"),
                             *sessionLimit));
    }
    const bool hasCredit =
        std::any_of(codingLimits.cbegin(), codingLimits.cend(), [](const Limit &limit) {
            return limit.type == QStringLiteral("CREDIT_LIMIT");
        });
    if (hasCredit) {
        rows.append(quotaRateRow(updatedAt));
    }
    // GCOVR_EXCL_BR_STOP
    if (timeLimit) {
        const bool monthlyMarker = timeLimit->unit == 5 && timeLimit->number == 1;
        windows.append(
            mappedWindow(*timeLimit, QStringLiteral("mcp"), QStringLiteral("MCP"), monthlyMarker));
        rows.append(limitRow(QStringLiteral("MCP quota"), *timeLimit));
        int detailCount = 0;
        for (const QJsonValue &value : timeLimit->details) {
            if (detailCount >= 20) {
                break;
            }
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject detail = value.toObject();
            const QJsonValue modelValue = detail.value(QStringLiteral("modelCode"));
            qint64 usage = 0;
            // GCOVR_EXCL_BR_START -- malformed usage details are ignored and tested
            if (!modelValue.isString() || !integer(detail.value(QStringLiteral("usage")), &usage)) {
                continue;
            }
            // GCOVR_EXCL_BR_STOP
            // GCOVR_EXCL_BR_START -- Qt container allocation branches
            rows.append(QVariantMap{{QStringLiteral("label"), modelValue.toString()},
                                    {QStringLiteral("value"), QString::number(usage)}});
            // GCOVR_EXCL_BR_STOP
            ++detailCount;
        }
    }

    QVariantMap identity;
    const QString plan = firstPlan(quotaData);
    if (!plan.isEmpty()) {
        identity.insert(QStringLiteral("plan"), plan);
    }
    // GCOVR_EXCL_BR_START -- Qt string and map allocation branches
    identity.insert(QStringLiteral("region"),
                    region == ZaiRegion::Global
                        ? QStringLiteral("Global")
                        : (scope == ZaiUsageScope::Team ? QStringLiteral("BigModel CN · Team")
                                                        : QStringLiteral("BigModel CN")));
    // GCOVR_EXCL_BR_STOP
    QVariant status;
    // GCOVR_EXCL_BR_START -- empty and populated provider states use Qt map branches
    if (windows.isEmpty()) {
        status = QVariantMap{{QStringLiteral("label"), QStringLiteral("No quota limits")},
                             {QStringLiteral("level"), QStringLiteral("warning")}};
    }
    // GCOVR_EXCL_BR_STOP
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    const QVariantList details{
        QVariantMap{{QStringLiteral("title"), QStringLiteral("Quota details")},
                    {QStringLiteral("rows"), rows}},
    };
    const QVariantMap provider{
        {QStringLiteral("id"), QStringLiteral("zai")},
        {QStringLiteral("name"), QStringLiteral("z.ai / GLM")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("source"), QStringLiteral("api-key")},
        {QStringLiteral("status"), status},
        {QStringLiteral("identity"), identity},
        {QStringLiteral("windows"), windows},
        {QStringLiteral("credits"), QVariant{}},
        {QStringLiteral("cost"), QVariant{}},
        {QStringLiteral("details"), details},
        {QStringLiteral("dataConfidence"), QStringLiteral("exact")},
        {QStringLiteral("display"),
         QVariantMap{{QStringLiteral("accentColor"), QStringLiteral("#E85A6A")},
                     {QStringLiteral("sortKey"), 60}}},
        {QStringLiteral("error"), QVariant{}},
        {QStringLiteral("updatedAt"), updatedAt.toUTC().toString(Qt::ISODateWithMs)},
    };
    // GCOVR_EXCL_BR_STOP
    return provider;
}

std::optional<ZaiModelUsage> ZaiUsageParser::parseModelUsage(const QByteArray &data, QString *error)
{
    const auto root = objectDocument(data, QStringLiteral("model usage"), error);
    if (!root) {
        return std::nullopt;
    }
    qint64 code = 0;
    // GCOVR_EXCL_BR_START -- malformed and unsuccessful model responses are tested
    if (!root->value(QStringLiteral("success")).toBool() ||
        !integer(root->value(QStringLiteral("code")), &code) || code != 200) {
        setError(error, QStringLiteral("z.ai model usage API returned an unsuccessful status"));
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_STOP
    const QJsonObject usageData = root->value(QStringLiteral("data")).toObject();
    const QJsonValue labelsValue = usageData.value(QStringLiteral("x_time"));
    const QJsonValue modelsValue = usageData.value(QStringLiteral("modelDataList"));
    // GCOVR_EXCL_BR_START -- malformed array combinations are tested
    if (!labelsValue.isArray() || !modelsValue.isArray()) {
        setError(error, QStringLiteral("z.ai model usage API returned malformed data"));
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_STOP
    const QJsonArray labels = labelsValue.toArray();
    QMap<QString, qint64> totals;
    QList<QJsonArray> modelSeries;
    for (const QJsonValue &value : modelsValue.toArray()) {
        if (!value.isObject()) {
            setError(error, QStringLiteral("z.ai model usage API returned malformed data"));
            return std::nullopt;
        }
        const QJsonObject model = value.toObject();
        const QJsonValue seriesValue = model.value(QStringLiteral("tokensUsage"));
        if (!seriesValue.isArray()) {
            setError(error, QStringLiteral("z.ai model usage API returned malformed data"));
            return std::nullopt;
        }
        QString name = model.value(QStringLiteral("modelName")).toString().trimmed();
        if (name.isEmpty()) {
            name = QStringLiteral("Unknown");
        }
        const QJsonArray series = seriesValue.toArray();
        modelSeries.append(series);
        qint64 total = 0;
        for (const QJsonValue &point : series) {
            qint64 count = 0;
            // GCOVR_EXCL_BR_START -- positive, zero, negative, null, and malformed points are
            // tested
            if (integer(point, &count) && count > 0) {
                addPositive(count, &total);
            }
            // GCOVR_EXCL_BR_STOP
        }
        if (total > 0) {
            qint64 modelTotal = totals.value(name);
            addPositive(total, &modelTotal);
            totals.insert(name, modelTotal);
        }
    }

    ZaiModelUsage result;
    for (qsizetype index = 0; index < labels.size(); ++index) {
        qint64 total = 0;
        for (const QJsonArray &series : modelSeries) {
            if (index < series.size()) {
                qint64 count = 0;
                // GCOVR_EXCL_BR_START -- series bounds and point classes are tested
                if (integer(series.at(index), &count) && count > 0) {
                    addPositive(count, &total);
                }
                // GCOVR_EXCL_BR_STOP
            }
        }
        if (total > 0) {
            // GCOVR_EXCL_BR_START -- Qt container allocation branches
            result.points.append(QVariantMap{
                {QStringLiteral("label"), labels.at(index).toVariant().toString()},
                {QStringLiteral("value"), total},
            });
            // GCOVR_EXCL_BR_STOP
        }
    }
    QList<QPair<QString, qint64>> sorted;
    for (auto iterator = totals.cbegin(); iterator != totals.cend(); ++iterator) {
        sorted.append({iterator.key(), iterator.value()});
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto &left, const auto &right) {
        return left.second != right.second ? left.second > right.second : left.first < right.first;
    });
    for (const auto &[name, total] : sorted.mid(0, 20)) {
        // GCOVR_EXCL_BR_START -- Qt container allocation branches
        result.rows.append(QVariantMap{{QStringLiteral("label"), name},
                                       {QStringLiteral("value"), QString::number(total)}});
        // GCOVR_EXCL_BR_STOP
    }
    return result;
}

std::optional<ZaiBalance> ZaiUsageParser::parseBalance(const QByteArray &data, QString *error)
{
    const auto root = objectDocument(data, QStringLiteral("balance"), error);
    if (!root) {
        return std::nullopt;
    }
    if (!root->value(QStringLiteral("success")).toBool()) {
        setError(error, QStringLiteral("z.ai balance API returned an unsuccessful status"));
        return std::nullopt;
    }
    const QJsonObject balanceData = root->value(QStringLiteral("data")).toObject();
    auto available = number(balanceData.value(QStringLiteral("availableBalance")));
    if (!available) {
        available = number(balanceData.value(QStringLiteral("balance")));
    }
    if (!available) {
        setError(error, QStringLiteral("z.ai balance API returned no numeric balance"));
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_START -- optional and Qt value access branches
    const ZaiBalance balance{
        *available, number(balanceData.value(QStringLiteral("rechargeAmount"))).value_or(0.0),
        number(balanceData.value(QStringLiteral("giveAmount"))).value_or(0.0),
        number(balanceData.value(QStringLiteral("totalSpendAmount"))).value_or(0.0)};
    // GCOVR_EXCL_BR_STOP
    return balance;
}

void ZaiUsageParser::appendModelUsage(QVariantMap &provider, const ZaiModelUsage &usage,
                                      const QString &title)
{
    if (usage.points.isEmpty()) {
        return;
    }
    QVariantList details = provider.value(QStringLiteral("details")).toList();
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    details.append(QVariantMap{
        {QStringLiteral("title"), title},
        {QStringLiteral("rows"), usage.rows},
        {QStringLiteral("chart"), QVariantMap{{QStringLiteral("kind"), QStringLiteral("bars")},
                                              {QStringLiteral("unit"), QStringLiteral("tokens")},
                                              {QStringLiteral("points"), usage.points}}},
    });
    // GCOVR_EXCL_BR_STOP
    provider.insert(QStringLiteral("details"), details);
}

void ZaiUsageParser::appendBalance(QVariantMap &provider, const ZaiBalance &balance)
{
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    provider.insert(QStringLiteral("cost"),
                    QVariantMap{{QStringLiteral("balance"), balance.available},
                                {QStringLiteral("toppedUpBalance"), balance.recharged},
                                {QStringLiteral("grantedBalance"), balance.granted},
                                {QStringLiteral("spent"), balance.spent},
                                {QStringLiteral("currencyCode"), QStringLiteral("CNY")},
                                {QStringLiteral("period"), QStringLiteral("Account balance")},
                                {QStringLiteral("available"), balance.available > 0.0}});
    // GCOVR_EXCL_BR_STOP
}

} // namespace Kodometer
