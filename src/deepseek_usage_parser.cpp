#include <kodometer/deepseek_usage_parser.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QVariantList>

#include <algorithm>
#include <cmath>

namespace Kodometer {
namespace {

struct Balance
{
    QString currency;
    double total = 0.0;
    double granted = 0.0;
    double toppedUp = 0.0;
};

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

std::optional<double> amount(const QJsonValue &value)
{
    bool valid = false;
    double result = 0.0;
    if (value.isString()) {
        const QString raw = value.toString().trimmed();
        result = raw.toDouble(&valid);
    }
    else if (value.isDouble()) {
        result = value.toDouble();
        valid = true;
    }
    if (!valid || !std::isfinite(result)) {
        return std::nullopt;
    }
    return result;
}

std::optional<Balance> parseBalance(const QJsonValue &value, QString *error)
{
    if (!value.isObject()) {
        setError(error,
                 QStringLiteral("DeepSeek balance API returned malformed balance information"));
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const QString currency =
        object.value(QStringLiteral("currency")).toString().trimmed().toUpper();
    static const QRegularExpression currencyPattern(QStringLiteral("^[A-Z]{3}$"));
    if (!currencyPattern.match(currency).hasMatch()) {
        setError(error, QStringLiteral("DeepSeek balance API returned an invalid currency"));
        return std::nullopt;
    }

    const auto total = amount(object.value(QStringLiteral("total_balance")));
    const auto granted = amount(object.value(QStringLiteral("granted_balance")));
    const auto toppedUp = amount(object.value(QStringLiteral("topped_up_balance")));
    if (!total || !granted || !toppedUp) {
        setError(error, QStringLiteral("DeepSeek balance API returned a non-numeric balance"));
        return std::nullopt;
    }
    return Balance{currency, *total, *granted, *toppedUp};
}

} // namespace

std::optional<QVariantMap> DeepSeekUsageParser::parse(const QByteArray &data,
                                                      const QDateTime &updatedAt, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("DeepSeek balance API returned invalid JSON"));
        return std::nullopt;
    }
    if (!document.isObject()) {
        setError(error, QStringLiteral("DeepSeek balance API returned an invalid object"));
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    const QJsonValue availabilityValue = root.value(QStringLiteral("is_available"));
    if (!availabilityValue.isBool()) {
        setError(error, QStringLiteral("DeepSeek balance API omitted availability"));
        return std::nullopt;
    }
    const QJsonValue balancesValue = root.value(QStringLiteral("balance_infos"));
    if (!balancesValue.isArray()) {
        setError(error, QStringLiteral("DeepSeek balance API omitted balance information"));
        return std::nullopt;
    }

    QList<Balance> balances;
    for (const QJsonValue &value : balancesValue.toArray()) {
        const auto balance = parseBalance(value, error);
        if (!balance) {
            return std::nullopt;
        }
        balances.append(*balance);
    }

    Balance selected{QStringLiteral("USD"), 0.0, 0.0, 0.0};
    if (!balances.isEmpty()) {
        auto iterator =
            std::find_if(balances.cbegin(), balances.cend(), [](const Balance &balance) {
                return balance.currency == QStringLiteral("USD") && balance.total > 0.0;
            });
        if (iterator == balances.cend()) {
            iterator = std::find_if(balances.cbegin(), balances.cend(),
                                    [](const Balance &balance) { return balance.total > 0.0; });
        }
        if (iterator == balances.cend()) {
            iterator = std::find_if(balances.cbegin(), balances.cend(), [](const Balance &balance) {
                return balance.currency == QStringLiteral("USD");
            });
        }
        selected = iterator == balances.cend() ? balances.first() : *iterator;
    }

    const bool available = !balances.isEmpty() && availabilityValue.toBool();
    QVariant status;
    if (selected.total <= 0.0) {
        status = QVariantMap{{QStringLiteral("label"), QStringLiteral("No credits")},
                             {QStringLiteral("level"), QStringLiteral("critical")}};
    }
    else if (!available) {
        status = QVariantMap{{QStringLiteral("label"), QStringLiteral("Unavailable for API calls")},
                             {QStringLiteral("level"), QStringLiteral("warning")}};
    }

    QVariantMap cost{{QStringLiteral("balance"), selected.total},
                     {QStringLiteral("grantedBalance"), selected.granted},
                     {QStringLiteral("toppedUpBalance"), selected.toppedUp},
                     {QStringLiteral("currencyCode"), selected.currency},
                     {QStringLiteral("period"), QStringLiteral("Account balance")},
                     {QStringLiteral("available"), available}};
    if (selected.currency == QStringLiteral("USD")) {
        cost.insert(QStringLiteral("balanceUSD"), selected.total);
    }

    return QVariantMap{
        {QStringLiteral("id"), QStringLiteral("deepseek")},
        {QStringLiteral("name"), QStringLiteral("DeepSeek")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("source"), QStringLiteral("api-key")},
        {QStringLiteral("status"), status},
        {QStringLiteral("identity"),
         QVariantMap{{QStringLiteral("plan"), QStringLiteral("API credits")}}},
        {QStringLiteral("windows"), QVariantList{}},
        {QStringLiteral("credits"), QVariant{}},
        {QStringLiteral("cost"), cost},
        {QStringLiteral("dataConfidence"), QStringLiteral("exact")},
        {QStringLiteral("display"),
         QVariantMap{{QStringLiteral("accentColor"), QStringLiteral("#4D6BFE")},
                     {QStringLiteral("sortKey"), 50}}},
        {QStringLiteral("error"), QVariant{}},
        {QStringLiteral("updatedAt"), updatedAt.toUTC().toString(Qt::ISODateWithMs)},
    };
}

} // namespace Kodometer
