#include <kodometer/xai_usage_parser.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QVariantList>

#include <cmath>

namespace Kodometer {
namespace {

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

std::optional<QJsonObject> objectDocument(const QByteArray &data, const QString &label,
                                          QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("xAI %1 API returned invalid JSON").arg(label));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("xAI %1 API returned an invalid object").arg(label));
        return std::nullopt;
    }
    return document.object();
}

} // namespace

std::optional<XaiBalance> XaiUsageParser::parseBalance(const QByteArray &data, QString *error)
{
    const auto root = objectDocument(data, QStringLiteral("balance"), error);
    if (!root) {
        return std::nullopt;
    }
    const QJsonValue rawValue =
        root->value(QStringLiteral("total")).toObject().value(QStringLiteral("val"));
    const QString raw =
        rawValue.isString() ? rawValue.toString().trimmed() : QString{}; // GCOVR_EXCL_BR_LINE
    static const QRegularExpression centsPattern(                        // GCOVR_EXCL_BR_LINE
        QStringLiteral("^-?[0-9]+(?:\\.[0-9]+)?$"));
    bool valid = false;
    const double cents = raw.toDouble(&valid);
    // GCOVR_EXCL_BR_START -- tested regex and QString accessors contain Qt branches
    if (raw.isEmpty() || !centsPattern.match(raw).hasMatch() || !valid || !std::isfinite(cents)) {
        setError(error, QStringLiteral("xAI balance API did not return a valid cent amount"));
        return std::nullopt;
    }
    // GCOVR_EXCL_BR_STOP
    return XaiBalance{-cents / 100.0};
}

std::optional<XaiUsageHistory> XaiUsageParser::parseHistory(const QByteArray &data, QString *error)
{
    const auto root = objectDocument(data, QStringLiteral("usage"), error);
    if (!root) {
        return std::nullopt;
    }
    const QJsonValue seriesValue = root->value(QStringLiteral("timeSeries"));
    if (!seriesValue.isArray()) {
        setError(error, QStringLiteral("xAI usage API returned no time series"));
        return std::nullopt;
    }

    XaiUsageHistory history;
    double total = 0.0;
    history.partial = root->value(QStringLiteral("limitReached")).toBool();
    for (const QJsonValue &seriesValueEntry : seriesValue.toArray()) {
        if (!seriesValueEntry.isObject()) {
            setError(error, QStringLiteral("xAI usage API returned malformed history"));
            return std::nullopt;
        }
        const QJsonValue pointsValue =
            seriesValueEntry.toObject().value(QStringLiteral("dataPoints"));
        if (!pointsValue.isArray()) {
            setError(error, QStringLiteral("xAI usage API returned malformed history"));
            return std::nullopt;
        }
        for (const QJsonValue &pointValue : pointsValue.toArray()) {
            if (!pointValue.isObject()) {
                setError(error, QStringLiteral("xAI usage API returned malformed history"));
                return std::nullopt;
            }
            const QJsonObject point = pointValue.toObject();
            const QDateTime timestamp = QDateTime::fromString(
                point.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
            const QJsonArray values = point.value(QStringLiteral("values")).toArray();
            const QJsonValue amountValue = values.isEmpty() ? QJsonValue{} : values.first();
            const double amount = amountValue.toDouble();
            // GCOVR_EXCL_BR_START -- tested Qt value accessors contain internal branches
            if (!timestamp.isValid() || !amountValue.isDouble() || !std::isfinite(amount) ||
                amount < 0.0) {
                setError(error, QStringLiteral("xAI usage API returned malformed history"));
                return std::nullopt;
            }
            // GCOVR_EXCL_BR_STOP
            total += amount;
            if (!std::isfinite(total)) {
                setError(error, QStringLiteral("xAI usage API spend aggregate overflowed"));
                return std::nullopt;
            }
            const QString day = timestamp.toUTC().date().toString(Qt::ISODate);
            history.daily.insert(day, history.daily.value(day) + amount);
        }
    }
    return history;
}

QVariantMap XaiUsageParser::provider(const XaiBalance &balance,
                                     const std::optional<XaiUsageHistory> &history,
                                     const QDateTime &updatedAt)
{
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    QVariantMap cost{{QStringLiteral("used"), balance.balanceUsd},
                     {QStringLiteral("usedUSD"), balance.balanceUsd},
                     {QStringLiteral("balanceUSD"), balance.balanceUsd},
                     {QStringLiteral("currencyCode"), QStringLiteral("USD")},
                     {QStringLiteral("period"), QStringLiteral("Prepaid credits")}};
    // GCOVR_EXCL_BR_STOP
    QString confidence = QStringLiteral("exact");
    if (history) {
        QVariantList daily;
        double total = 0.0;
        const QDate end = updatedAt.toUTC().date();
        const QDate start = end.addDays(-29);
        const QString today = end.toString(Qt::ISODate);
        for (auto point = history->daily.cbegin(); point != history->daily.cend(); ++point) {
            const QDate date = QDate::fromString(point.key(), Qt::ISODate);
            if (date < start || date > end) {
                continue;
            }
            // GCOVR_EXCL_BR_START -- Qt container allocation branches
            daily.append(QVariantMap{{QStringLiteral("label"), point.key()},
                                     {QStringLiteral("value"), point.value()}});
            // GCOVR_EXCL_BR_STOP
            total += point.value();
        }
        if (history->daily.contains(today)) {
            cost.insert(QStringLiteral("todayUSD"), history->daily.value(today));
        }
        cost.insert(QStringLiteral("last30DaysUSD"), total);
        cost.insert(QStringLiteral("historyPartial"), history->partial);
        cost.insert(QStringLiteral("daily"), daily);
        cost.insert(QStringLiteral("historyEndDate"), today);
        cost.insert(QStringLiteral("historyIncludesCurrentDay"), true);
        if (history->partial) {
            confidence = QStringLiteral("estimated");
        }
    }

    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    return QVariantMap{
        {QStringLiteral("id"), QStringLiteral("xai")},
        {QStringLiteral("name"), QStringLiteral("xAI")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("source"), QStringLiteral("api-key")},
        {QStringLiteral("status"), QVariant{}},
        {QStringLiteral("identity"),
         QVariantMap{{QStringLiteral("plan"), QStringLiteral("Management API")}}},
        {QStringLiteral("windows"), QVariantList{}},
        {QStringLiteral("credits"), QVariant{}},
        {QStringLiteral("cost"), cost},
        {QStringLiteral("dataConfidence"), confidence},
        {QStringLiteral("display"),
         QVariantMap{{QStringLiteral("accentColor"), QStringLiteral("#8E8E93")},
                     {QStringLiteral("sortKey"), 30}}},
        {QStringLiteral("error"), QVariant{}},
        {QStringLiteral("updatedAt"), updatedAt.toUTC().toString(Qt::ISODateWithMs)},
    };
    // GCOVR_EXCL_BR_STOP
}

} // namespace Kodometer
