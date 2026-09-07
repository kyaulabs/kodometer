#include <kodometer/codex_usage_parser.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
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
    return std::isfinite(result) ? std::optional<double>(result) : std::nullopt;
}

QString isoTimestamp(double epochSeconds)
{
    if (std::floor(epochSeconds) != epochSeconds) {
        return {};
    }
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(epochSeconds), QTimeZone::UTC)
        .toString(Qt::ISODateWithMs);
}

std::optional<QVariantMap> window(const QJsonValue &value, const QString &kind,
                                  const QString &label)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const auto used = number(object.value(QStringLiteral("used_percent")));
    const auto resetAt = number(object.value(QStringLiteral("reset_at")));
    const auto duration = number(object.value(QStringLiteral("limit_window_seconds")));
    if (!used || !resetAt || !duration) {
        return std::nullopt;
    }
    const QString resetTimestamp = isoTimestamp(*resetAt);
    if (resetTimestamp.isEmpty()) {
        return std::nullopt;
    }

    const double boundedUsed = std::clamp(*used, 0.0, 100.0);
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    return QVariantMap{
        {QStringLiteral("kind"), kind},
        {QStringLiteral("label"), label},
        {QStringLiteral("usedPercent"), boundedUsed},
        {QStringLiteral("remainingPercent"), 100.0 - boundedUsed},
        {QStringLiteral("resetAt"), resetTimestamp},
        {QStringLiteral("windowSeconds"), *duration},
    };
    // GCOVR_EXCL_BR_STOP
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

void appendWindow(QVariantList &windows, const QJsonObject &limits, const QString &key,
                  const QString &kind, const QString &label)
{
    const auto mapped = window(limits.value(key), kind, label);
    if (mapped) {
        windows.append(*mapped);
    }
}

QVariant credits(const QJsonObject &root)
{
    const QJsonObject object = root.value(QStringLiteral("credits")).toObject();
    const bool hasCredits =
        object.value(QStringLiteral("has_credits")).toBool();                  // GCOVR_EXCL_BR_LINE
    const bool unlimited = object.value(QStringLiteral("unlimited")).toBool(); // GCOVR_EXCL_BR_LINE
    if (!hasCredits) {
        return {};
    }
    if (unlimited) {
        return {};
    }
    const auto balance = number(object.value(QStringLiteral("balance")));
    if (!balance) {
        return {};
    }
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    return QVariantMap{
        {QStringLiteral("remaining"), *balance},
        {QStringLiteral("unit"), QStringLiteral("credits")},
    };
    // GCOVR_EXCL_BR_STOP
}

} // namespace

std::optional<QVariantMap> CodexUsageParser::parse(const QByteArray &data,
                                                   const CodexCredentials &credentials,
                                                   const QDateTime &updatedAt, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Codex usage API returned invalid JSON"));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("Codex usage API returned an invalid object"));
        return std::nullopt;
    }
    const QJsonObject root = document.object();

    QVariantList windows;
    const QJsonObject rateLimit = root.value(QStringLiteral("rate_limit")).toObject();
    appendWindow(windows, rateLimit, QStringLiteral("primary_window"), QStringLiteral("session"),
                 QStringLiteral("Session"));
    appendWindow(windows, rateLimit, QStringLiteral("secondary_window"), QStringLiteral("weekly"),
                 QStringLiteral("Weekly"));

    const QJsonArray additional = root.value(QStringLiteral("additional_rate_limits")).toArray();
    for (const QJsonValue &entryValue : additional) {
        if (!entryValue.isObject()) {
            continue;
        }
        const QJsonObject entry = entryValue.toObject();
        QString feature = entry.value(QStringLiteral("metered_feature")).toString().trimmed();
        const QString limitName = entry.value(QStringLiteral("limit_name")).toString().trimmed();
        if (feature.isEmpty()) {
            feature = limitName;
        }
        feature = slug(feature);
        if (feature.isEmpty()) {
            continue;
        }
        const QString label = limitName.isEmpty() ? formatPlan(feature) : limitName;
        const QJsonObject limits = entry.value(QStringLiteral("rate_limit")).toObject();
        appendWindow(windows, limits, QStringLiteral("primary_window"),
                     QStringLiteral("model-") + feature, label);
        appendWindow(windows, limits, QStringLiteral("secondary_window"),
                     QStringLiteral("model-") + feature + QStringLiteral("-weekly"),
                     label + QStringLiteral(" Weekly"));
    }

    QVariantMap identity;
    QString accountId = root.value(QStringLiteral("account_id")).toString().trimmed();
    if (accountId.isEmpty()) {
        accountId = credentials.accountId;
    }
    if (!accountId.isEmpty()) {
        identity.insert(QStringLiteral("accountId"), accountId);
    }
    const QString email = credentials.redactedEmail();
    if (!email.isEmpty()) {
        identity.insert(QStringLiteral("accountEmail"), email);
    }
    const QString plan = formatPlan(root.value(QStringLiteral("plan_type")).toString());
    if (!plan.isEmpty()) {
        identity.insert(QStringLiteral("plan"), plan);
    }

    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    QVariantMap provider{
        {QStringLiteral("id"), QStringLiteral("codex")},
        {QStringLiteral("name"), QStringLiteral("Codex")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("source"),
         credentials.apiKey ? QStringLiteral("api-key") : QStringLiteral("oauth")},
        {QStringLiteral("status"), QVariant{}},
        {QStringLiteral("identity"), identity},
        {QStringLiteral("windows"), windows},
        {QStringLiteral("credits"), credits(root)},
        {QStringLiteral("cost"), QVariant{}},
        {QStringLiteral("display"),
         QVariantMap{{QStringLiteral("accentColor"), QStringLiteral("#49A3B0")},
                     {QStringLiteral("sortKey"), 0}}},
        {QStringLiteral("error"), QVariant{}},
        {QStringLiteral("updatedAt"), updatedAt.toUTC().toString(Qt::ISODateWithMs)},
    };
    // GCOVR_EXCL_BR_STOP
    // Optional summary returned by wham/usage; reading it never redeems a reset credit.
    const QJsonValue available = root.value(QStringLiteral("rate_limit_reset_credits"))
                                     .toObject()
                                     .value(QStringLiteral("available_count"));
    const int count = available.toInt(-1);
    if (count >= 0) {
        provider.insert(QStringLiteral("bankedResets"), count);
    }
    return provider;
}

QString CodexUsageParser::formatPlan(const QString &plan)
{
    const QString normalized = plan.trimmed().toLower();
    if (normalized == QLatin1String("pro")) {
        return QStringLiteral("Pro ($200/month)");
    }
    const QStringList words =
        normalized.split(QRegularExpression(QStringLiteral("[\\s_-]+")), Qt::SkipEmptyParts);
    if (words.join(QString{}) == QLatin1String("prolite")) {
        return QStringLiteral("Pro-Lite ($100/month)");
    }
    QStringList formatted;
    formatted.reserve(words.size());
    for (QString word : words) {
        if (!word.isEmpty()) {
            word[0] = word.at(0).toUpper();
            formatted.append(word);
        }
    }
    return formatted.join(QLatin1Char(' '));
}

} // namespace Kodometer
