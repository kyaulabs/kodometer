#include <kodometer/gemini_usage_parser.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QTimeZone>
#include <QVariantList>

#include <algorithm>
#include <cmath>

namespace Kodometer {
namespace {

struct ModelQuota
{
    double remainingFraction = 0.0;
    QString resetAt;
};

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

QString nonEmpty(const QJsonValue &value)
{
    return value.isString() ? value.toString().trimmed() : QString{}; // GCOVR_EXCL_BR_LINE
}

QString timestamp(const QString &value)
{
    if (value.isEmpty()) {
        return {};
    }
    const QDateTime parsed = QDateTime::fromString(value, Qt::ISODate);
    // GCOVR_EXCL_BR_START -- Qt date formatting allocation branches
    const QString result =
        parsed.isValid() ? parsed.toUTC().toString(Qt::ISODateWithMs) : QString{};
    // GCOVR_EXCL_BR_STOP
    return result;
}

QString planFor(const QString &tier, const QString &hostedDomain, const QString &paidTierName)
{
    if (!paidTierName.isEmpty()) {
        return paidTierName;
    }
    if (tier == QStringLiteral("standard-tier")) {
        return QStringLiteral("Paid");
    }
    if (tier == QStringLiteral("free-tier")) {
        return hostedDomain.isEmpty() ? QStringLiteral("Free") : QStringLiteral("Workspace");
    }
    if (tier == QStringLiteral("legacy-tier")) {
        return QStringLiteral("Legacy");
    }
    return {};
}

QString projectFrom(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString().trimmed();
    }
    if (!value.isObject()) {
        return {};
    }
    const QJsonObject project = value.toObject();
    QString id = nonEmpty(project.value(QStringLiteral("id")));
    if (id.isEmpty()) {
        id = nonEmpty(project.value(QStringLiteral("projectId")));
    }
    return id;
}

bool unsupportedTier(const QJsonObject &root)
{
    const QJsonArray tiers = root.value(QStringLiteral("ineligibleTiers")).toArray();
    for (const QJsonValue &value : tiers) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject tier = value.toObject();
        const QByteArray signal =
            (nonEmpty(tier.value(QStringLiteral("reasonCode"))) + QLatin1Char(' ') +
             nonEmpty(tier.value(QStringLiteral("reasonMessage"))))
                .toUtf8();
        if (GeminiUsageParser::isConsumerTierDeprecation(signal)) {
            return true;
        }
    }
    return false;
}

QString quotaTier(const QString &modelId)
{
    const QString lower = modelId.toLower();
    if (lower.contains(QStringLiteral("flash-lite"))) {
        return QStringLiteral("flash-lite");
    }
    if (lower.contains(QStringLiteral("flash"))) {
        return QStringLiteral("flash");
    }
    if (lower.contains(QStringLiteral("pro"))) {
        return QStringLiteral("pro");
    }
    return {};
}

void appendWindow(QVariantList &windows, const QMap<QString, ModelQuota> &tiers,
                  const QString &tier, const QString &kind, const QString &label)
{
    const auto quota = tiers.constFind(tier);
    if (quota == tiers.cend()) {
        return;
    }
    const double remaining = std::clamp(quota->remainingFraction, 0.0, 1.0) * 100.0;
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    QVariantMap window{{QStringLiteral("kind"), kind},
                       {QStringLiteral("label"), label},
                       {QStringLiteral("usedPercent"), 100.0 - remaining},
                       {QStringLiteral("remainingPercent"), remaining},
                       {QStringLiteral("windowSeconds"), 24 * 60 * 60}};
    // GCOVR_EXCL_BR_STOP
    const QString resetAt = timestamp(quota->resetAt);
    if (!resetAt.isEmpty()) {
        window.insert(QStringLiteral("resetAt"), resetAt);
    }
    windows.append(window);
}

} // namespace

std::optional<GeminiCodeAssistStatus>
GeminiUsageParser::parseCodeAssist(const QByteArray &data, const QString &hostedDomain,
                                   QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Gemini Code Assist API returned invalid JSON"));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("Gemini Code Assist API returned an invalid object"));
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    const QString paidTierName =
        nonEmpty(root.value(QStringLiteral("paidTier")).toObject().value(QStringLiteral("name")));

    GeminiCodeAssistStatus status;
    status.projectId = projectFrom(root.value(QStringLiteral("cloudaicompanionProject")));
    status.tier =
        nonEmpty(root.value(QStringLiteral("currentTier")).toObject().value(QStringLiteral("id")));
    status.plan = planFor(status.tier, hostedDomain, paidTierName);
    status.consumerClientUnsupported =
        paidTierName.isEmpty() && hostedDomain.isEmpty() && unsupportedTier(root);
    return status;
}

std::optional<QVariantMap> GeminiUsageParser::parseQuota(const QByteArray &data,
                                                         const GeminiCredentials &credentials,
                                                         const GeminiCodeAssistStatus &status,
                                                         const QDateTime &updatedAt, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Gemini quota API returned invalid JSON"));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("Gemini quota API returned an invalid object"));
        return std::nullopt;
    }

    QMap<QString, ModelQuota> models;
    const QJsonArray buckets = document.object().value(QStringLiteral("buckets")).toArray();
    for (const QJsonValue &value : buckets) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject bucket = value.toObject();
        const QString modelId = nonEmpty(bucket.value(QStringLiteral("modelId")));
        const QJsonValue fractionValue = bucket.value(QStringLiteral("remainingFraction"));
        const double fraction = fractionValue.toDouble();
        if (modelId.isEmpty() || !fractionValue.isDouble() ||
            !std::isfinite(fraction)) { // GCOVR_EXCL_BR_LINE
            continue;
        }
        const ModelQuota candidate{fraction, nonEmpty(bucket.value(QStringLiteral("resetTime")))};
        const auto existing = models.constFind(modelId);
        // GCOVR_EXCL_BR_START -- Qt map iterator comparison branches
        if (existing == models.cend() || fraction < existing->remainingFraction) {
            models.insert(modelId, candidate);
        }
        // GCOVR_EXCL_BR_STOP
    }

    QMap<QString, ModelQuota> tiers;
    for (auto model = models.cbegin(); model != models.cend(); ++model) {
        const QString tier = quotaTier(model.key());
        if (tier.isEmpty()) {
            continue;
        }
        const auto existing = tiers.constFind(tier);
        // GCOVR_EXCL_BR_START -- Qt map iterator comparison branches
        if (existing == tiers.cend() || model->remainingFraction < existing->remainingFraction) {
            tiers.insert(tier, model.value());
        }
        // GCOVR_EXCL_BR_STOP
    }

    QVariantList windows;
    appendWindow(windows, tiers, QStringLiteral("pro"), QStringLiteral("model-pro-daily"),
                 QStringLiteral("Pro"));
    appendWindow(windows, tiers, QStringLiteral("flash"), QStringLiteral("model-flash-daily"),
                 QStringLiteral("Flash"));
    appendWindow(windows, tiers, QStringLiteral("flash-lite"),
                 QStringLiteral("model-flash-lite-daily"), QStringLiteral("Flash Lite"));
    if (windows.isEmpty()) {
        setError(error, QStringLiteral("Gemini quota API returned no usable model limits"));
        return std::nullopt;
    }

    QVariantMap identity;
    const QString email = credentials.redactedEmail();
    if (!email.isEmpty()) {
        identity.insert(QStringLiteral("accountEmail"), email);
    }
    if (!status.plan.isEmpty()) {
        identity.insert(QStringLiteral("plan"), status.plan);
    }

    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    return QVariantMap{
        {QStringLiteral("id"), QStringLiteral("gemini")},
        {QStringLiteral("name"), QStringLiteral("Gemini")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("source"), QStringLiteral("oauth")},
        {QStringLiteral("status"), QVariant{}},
        {QStringLiteral("identity"), identity},
        {QStringLiteral("windows"), windows},
        {QStringLiteral("credits"), QVariant{}},
        {QStringLiteral("cost"), QVariant{}},
        {QStringLiteral("display"),
         QVariantMap{{QStringLiteral("accentColor"), QStringLiteral("#4285F4")},
                     {QStringLiteral("sortKey"), 20}}},
        {QStringLiteral("error"), QVariant{}},
        {QStringLiteral("updatedAt"), updatedAt.toUTC().toString(Qt::ISODateWithMs)},
    };
    // GCOVR_EXCL_BR_STOP
}

QString GeminiUsageParser::discoverProject(const QByteArray &data)
{
    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (!document.isObject()) {
        return {};
    }
    const QJsonArray projects = document.object().value(QStringLiteral("projects")).toArray();
    for (const QJsonValue &value : projects) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject project = value.toObject();
        const QString projectId = nonEmpty(project.value(QStringLiteral("projectId")));
        if (projectId.startsWith(QStringLiteral("gen-lang-client"))) {
            return projectId;
        }
        // GCOVR_EXCL_BR_START -- Qt JSON lookup allocation branches
        if (!projectId.isEmpty() && project.value(QStringLiteral("labels"))
                                        .toObject()
                                        .contains(QStringLiteral("generative-language"))) {
            return projectId;
        }
        // GCOVR_EXCL_BR_STOP
    }
    return {};
}

bool GeminiUsageParser::isConsumerTierDeprecation(const QByteArray &data)
{
    const QString normalized = QString::fromUtf8(data).toLower();
    // GCOVR_EXCL_BR_START -- QString search allocation branches
    const bool detected = normalized.contains(QStringLiteral("unsupported_client")) ||
                          normalized.contains(QStringLiteral("ineligibletiererror")) ||
                          (normalized.contains(QStringLiteral("no longer supported")) &&
                           normalized.contains(QStringLiteral("gemini code assist"))) ||
                          (normalized.contains(QStringLiteral("migrate")) &&
                           normalized.contains(QStringLiteral("antigravity")) &&
                           normalized.contains(QStringLiteral("gemini")));
    // GCOVR_EXCL_BR_STOP
    return detected;
}

} // namespace Kodometer
