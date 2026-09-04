#include <kodometer/claude_usage_parser.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <QVariantList>

#include <algorithm>
#include <cmath>

namespace Kodometer {
namespace {

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

std::optional<double> number(const QJsonValue &value)
{
    double result = 0.0;
    if (value.isDouble()) {
        result = value.toDouble();
    }
    else if (value.isString()) {
        bool valid = false;
        result = value.toString().trimmed().toDouble(&valid);
        if (!valid) {
            return std::nullopt;
        }
    }
    else {
        return std::nullopt;
    }
    return std::isfinite(result) ? std::optional<double>(result)
                                 : std::nullopt; // GCOVR_EXCL_BR_LINE
}

QString nonEmpty(const QJsonValue &value)
{
    return value.isString() ? value.toString().trimmed() : QString{}; // GCOVR_EXCL_BR_LINE
}

QString isoTimestamp(const QJsonValue &value)
{
    const QString timestamp = nonEmpty(value);
    if (timestamp.isEmpty()) {
        return {};
    }
    const QDateTime parsed = QDateTime::fromString(timestamp, Qt::ISODate);
    // GCOVR_EXCL_BR_START -- Qt string allocation branches
    const QString result =
        parsed.isValid() ? parsed.toUTC().toString(Qt::ISODateWithMs) : QString{};
    // GCOVR_EXCL_BR_STOP
    return result;
}

QString slug(const QString &value)
{
    QString result = value.trimmed().toLower();
    result.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    while (result.startsWith(QLatin1Char('-'))) {
        result.remove(0, 1);
    }
    while (result.endsWith(QLatin1Char('-'))) {
        result.chop(1);
    }
    return result;
}

std::optional<QVariantMap> mapWindow(const QJsonValue &value, const QString &kind,
                                     const QString &label, int windowSeconds)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const auto utilization = number(object.value(QStringLiteral("utilization")));
    if (!utilization) {
        return std::nullopt;
    }
    const double used = std::clamp(*utilization, 0.0, 100.0);
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    QVariantMap window{
        {QStringLiteral("kind"), kind},
        {QStringLiteral("label"), label},
        {QStringLiteral("usedPercent"), used},
        {QStringLiteral("remainingPercent"), 100.0 - used},
        {QStringLiteral("windowSeconds"), windowSeconds},
    };
    // GCOVR_EXCL_BR_STOP
    const QString resetAt = isoTimestamp(object.value(QStringLiteral("resets_at")));
    if (!resetAt.isEmpty()) {
        window.insert(QStringLiteral("resetAt"), resetAt);
    }
    return window;
}

void appendWindow(QVariantList &windows, const QJsonObject &root, const QString &key,
                  const QString &kind, const QString &label, int windowSeconds)
{
    const auto window = mapWindow(root.value(key), kind, label, windowSeconds);
    if (window) {
        windows.append(*window);
    }
}

QJsonValue firstValue(const QJsonObject &root, const QStringList &keys)
{
    for (const QString &key : keys) {
        // GCOVR_EXCL_BR_START -- Qt value allocation branches
        const QJsonValue value = root.value(key);
        if (!value.isUndefined() && !value.isNull()) {
            return value;
        }
        // GCOVR_EXCL_BR_STOP
    }
    return {};
}

void appendScopedWindows(QVariantList &windows, const QJsonArray &limits)
{
    QSet<QString> seen;
    for (const QJsonValue &limitValue : limits) {
        if (!limitValue.isObject()) {
            continue;
        }
        const QJsonObject limit = limitValue.toObject();
        // GCOVR_EXCL_BR_START -- Qt string allocation branches
        if (nonEmpty(limit.value(QStringLiteral("kind"))) != QStringLiteral("weekly_scoped") ||
            nonEmpty(limit.value(QStringLiteral("group"))) != QStringLiteral("weekly")) {
            continue;
        }
        // GCOVR_EXCL_BR_STOP
        const auto percent = number(limit.value(QStringLiteral("percent")));
        const QJsonObject model = limit.value(QStringLiteral("scope"))
                                      .toObject()
                                      .value(QStringLiteral("model"))
                                      .toObject();
        const QString modelName = nonEmpty(model.value(QStringLiteral("display_name")));
        const QString modelId = nonEmpty(model.value(QStringLiteral("id")));
        if (!percent || modelName.isEmpty()) { // GCOVR_EXCL_BR_LINE
            continue;
        }
        const QString nameSlug = slug(modelName);
        const QString idSlug = slug(modelId);
        // GCOVR_EXCL_BR_START -- Qt string allocation branches
        if (nameSlug == QStringLiteral("all-models") || idSlug == QStringLiteral("all-models") ||
            idSlug.endsWith(QStringLiteral("-all-models"))) {
            continue;
        }
        // GCOVR_EXCL_BR_STOP
        QString identifier = idSlug.isEmpty() ? nameSlug : idSlug; // GCOVR_EXCL_BR_LINE
        if (identifier.startsWith(QStringLiteral("claude-"))) {
            identifier.remove(0, 7);
        }
        if (identifier.isEmpty() || seen.contains(identifier)) {
            continue;
        }
        seen.insert(identifier);
        const double used = std::clamp(*percent, 0.0, 100.0);
        // GCOVR_EXCL_BR_START -- Qt container allocation branches
        QVariantMap window{
            {QStringLiteral("kind"),
             QStringLiteral("model-") + identifier + QStringLiteral("-weekly")},
            {QStringLiteral("label"), modelName + QStringLiteral(" only")},
            {QStringLiteral("usedPercent"), used},
            {QStringLiteral("remainingPercent"), 100.0 - used},
            {QStringLiteral("windowSeconds"), 7 * 24 * 60 * 60},
        };
        // GCOVR_EXCL_BR_STOP
        const QString resetAt = isoTimestamp(limit.value(QStringLiteral("resets_at")));
        if (!resetAt.isEmpty()) {
            window.insert(QStringLiteral("resetAt"), resetAt);
        }
        windows.append(window);
    }
}

QVariantMap mapCost(const QJsonObject &root, const QString &plan)
{
    const QJsonObject extra = root.value(QStringLiteral("extra_usage")).toObject();
    if (!extra.value(QStringLiteral("is_enabled")).toBool()) {
        return {};
    }
    const auto used = number(extra.value(QStringLiteral("used_credits")));
    const auto limit = number(extra.value(QStringLiteral("monthly_limit")));
    if (!used || !limit) { // GCOVR_EXCL_BR_LINE
        return {};
    }
    QString currency = nonEmpty(extra.value(QStringLiteral("currency"))).toUpper();
    if (currency.isEmpty()) {
        currency = QStringLiteral("USD");
    }
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    QVariantMap cost{
        {QStringLiteral("used"), *used / 100.0},
        {QStringLiteral("limit"), *limit / 100.0},
        {QStringLiteral("currencyCode"), currency},
        {QStringLiteral("period"), plan == QStringLiteral("Claude Enterprise")
                                       ? QStringLiteral("Spend limit")
                                       : QStringLiteral("Monthly cap")},
    };
    // GCOVR_EXCL_BR_STOP
    if (currency == QStringLiteral("USD")) {
        cost.insert(QStringLiteral("usedUSD"), *used / 100.0);
        cost.insert(QStringLiteral("limitUSD"), *limit / 100.0);
    }
    const auto utilization = number(extra.value(QStringLiteral("utilization")));
    if (utilization) {
        cost.insert(QStringLiteral("usedPercent"), std::clamp(*utilization, 0.0, 100.0));
    }
    return cost;
}

} // namespace

std::optional<QVariantMap> ClaudeUsageParser::parse(const QByteArray &data,
                                                    const ClaudeCredentials &credentials,
                                                    const QDateTime &updatedAt, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Claude usage API returned invalid JSON"));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("Claude usage API returned an invalid object"));
        return std::nullopt;
    }
    const QJsonObject root = document.object();

    QVariantList windows;
    appendWindow(windows, root, QStringLiteral("five_hour"), QStringLiteral("session"),
                 QStringLiteral("Session"), 5 * 60 * 60);
    appendWindow(windows, root, QStringLiteral("seven_day"), QStringLiteral("weekly"),
                 QStringLiteral("Weekly"), 7 * 24 * 60 * 60);
    appendWindow(windows, root, QStringLiteral("seven_day_sonnet"),
                 QStringLiteral("model-sonnet-weekly"), QStringLiteral("Sonnet Weekly"),
                 7 * 24 * 60 * 60);
    appendWindow(windows, root, QStringLiteral("seven_day_opus"),
                 QStringLiteral("model-opus-weekly"), QStringLiteral("Opus Weekly"),
                 7 * 24 * 60 * 60);
    appendWindow(windows, root, QStringLiteral("seven_day_oauth_apps"),
                 QStringLiteral("oauth-apps-weekly"), QStringLiteral("OAuth Apps Weekly"),
                 7 * 24 * 60 * 60);
    appendScopedWindows(windows, root.value(QStringLiteral("limits")).toArray());
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    const auto routines = mapWindow(
        firstValue(root,
                   {QStringLiteral("seven_day_routines"),
                    QStringLiteral("seven_day_claude_routines"), QStringLiteral("claude_routines"),
                    QStringLiteral("routines"), QStringLiteral("routine"),
                    QStringLiteral("seven_day_cowork"), QStringLiteral("cowork")}),
        QStringLiteral("claude-routines"), QStringLiteral("Daily Routines"), 7 * 24 * 60 * 60);
    // GCOVR_EXCL_BR_STOP
    if (routines) {
        windows.append(*routines);
    }

    const QString plan = formatPlan(credentials.subscriptionType, credentials.rateLimitTier);
    const QVariantMap cost = mapCost(root, plan);
    const bool needsSpendWindow =
        windows.isEmpty() && cost.contains(QStringLiteral("usedPercent")); // GCOVR_EXCL_BR_LINE
    if (needsSpendWindow) {
        const double used = cost.value(QStringLiteral("usedPercent")).toDouble();
        // GCOVR_EXCL_BR_START -- Qt container allocation branches
        windows.append(QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("spend-limit")},
            {QStringLiteral("label"), QStringLiteral("Spend limit")},
            {QStringLiteral("usedPercent"), used},
            {QStringLiteral("remainingPercent"), 100.0 - used},
        });
        // GCOVR_EXCL_BR_STOP
    }
    const bool hasNoUsage = windows.isEmpty() && cost.isEmpty(); // GCOVR_EXCL_BR_LINE
    if (hasNoUsage) {
        setError(error, QStringLiteral("Claude usage API returned no usable limits"));
        return std::nullopt;
    }

    QVariantMap identity;
    if (!plan.isEmpty()) {
        identity.insert(QStringLiteral("plan"), plan);
    }

    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    return QVariantMap{
        {QStringLiteral("id"), QStringLiteral("claude")},
        {QStringLiteral("name"), QStringLiteral("Claude")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("source"), QStringLiteral("oauth")},
        {QStringLiteral("status"), QVariant{}},
        {QStringLiteral("identity"), identity},
        {QStringLiteral("windows"), windows},
        {QStringLiteral("credits"), QVariant{}},
        {QStringLiteral("cost"), cost.isEmpty() ? QVariant{} : QVariant::fromValue(cost)},
        {QStringLiteral("display"),
         QVariantMap{{QStringLiteral("accentColor"), QStringLiteral("#D97757")},
                     {QStringLiteral("sortKey"), 10}}},
        {QStringLiteral("error"), QVariant{}},
        {QStringLiteral("updatedAt"), updatedAt.toUTC().toString(Qt::ISODateWithMs)},
    };
    // GCOVR_EXCL_BR_STOP
}

QString ClaudeUsageParser::formatPlan(const QString &subscriptionType, const QString &rateLimitTier)
{
    const QString combined =
        subscriptionType.trimmed().toLower() + QLatin1Char(' ') + rateLimitTier.trimmed().toLower();
    const QStringList words =
        combined.split(QRegularExpression(QStringLiteral("[^a-z0-9]+")), Qt::SkipEmptyParts);
    const auto contains = [&words](const QString &word) { return words.contains(word); };
    if (contains(QStringLiteral("max"))) {
        QString label = QStringLiteral("Claude Max");
        for (qsizetype index = 0; index + 1 < words.size(); ++index) {
            if (words.at(index) != QStringLiteral("max")) {
                continue;
            }
            const QString multiplier = words.at(index + 1);
            if (QRegularExpression(QStringLiteral("^[0-9]+x$")).match(multiplier).hasMatch()) {
                label += QLatin1Char(' ') + multiplier;
                break;
            }
        }
        return label;
    }
    if (contains(QStringLiteral("pro"))) {
        return QStringLiteral("Claude Pro");
    }
    if (contains(QStringLiteral("team"))) {
        return QStringLiteral("Claude Team");
    }
    if (contains(QStringLiteral("enterprise"))) {
        return QStringLiteral("Claude Enterprise");
    }
    if (contains(QStringLiteral("ultra"))) {
        return QStringLiteral("Claude Ultra");
    }
    return {};
}

} // namespace Kodometer
