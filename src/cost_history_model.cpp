#include <kodometer/cost_history_model.hpp>
#include <kodometer/presentation_formatter.hpp>

#include <QDate>

#include <algorithm>
#include <cmath>
#include <optional>

namespace Kodometer {
namespace {

QDate calendarDate(const QVariant &value)
{
    const QString text = value.toString();
    const QDate date = QDate::fromString(text, Qt::ISODate);
    return date.toString(Qt::ISODate) == text ? date : QDate{};
}

std::optional<double> amount(const QVariant &value)
{
    if (value.isNull()) {
        return std::nullopt;
    }
    switch (value.metaType().id()) {
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        break;
    default:
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0.0) {
        return std::nullopt;
    }
    return number;
}

QString money(double value)
{
    return value > 0.0 && value < 0.01 ? QStringLiteral("<$0.01")
                                       : PresentationFormatter::usdLabel(value);
}

} // namespace

CostHistoryModel::CostHistoryModel(QObject *parent) : QObject(parent) {}

QVariantMap CostHistoryModel::cost() const
{
    return m_cost;
}

int CostHistoryModel::days() const noexcept
{
    return m_days;
}

QVariantMap CostHistoryModel::view() const
{
    return m_view;
}

void CostHistoryModel::setCost(const QVariantMap &cost)
{
    const QVariantMap view = buildView(cost, m_days);
    // QVariant equality can equate differently typed values; validation must run first.
    if (m_cost == cost && m_view == view) {
        return;
    }
    m_cost = cost;
    setView(view);
    emit costChanged();
}

void CostHistoryModel::setDays(int days)
{
    const int bounded = days == 7 ? 7 : 30;
    if (m_days == bounded) {
        return;
    }
    m_days = bounded;
    setView(buildView(m_cost, m_days));
    emit daysChanged();
}

void CostHistoryModel::setView(const QVariantMap &view)
{
    if (m_view != view) {
        m_view = view;
        emit viewChanged();
    }
}

QVariantMap CostHistoryModel::buildView(const QVariantMap &cost, int days)
{
    const QVariant daily = cost.value(QStringLiteral("daily"));
    const QDate end = calendarDate(cost.value(QStringLiteral("historyEndDate")));
    const QString currency = cost.value(QStringLiteral("currencyCode")).toString();
    if (currency != QLatin1String("USD") || daily.isNull() ||
        daily.metaType().id() != QMetaType::QVariantList || !end.isValid()) {
        return {};
    }
    const QVariantList entries = daily.toList();
    // Bound work independently of the network response limit; the UI always has <= 30 bars.
    if (entries.size() > 366) {
        return {};
    }
    const QDate start = end.addDays(1 - days);
    if (start.toString(Qt::ISODate).isEmpty()) {
        return {};
    }
    QMap<QDate, double> values;
    for (const QVariant &entry : entries) {
        const QVariantMap row = entry.toMap();
        const QDate date = calendarDate(row.value(QStringLiteral("label")));
        const auto value = amount(row.value(QStringLiteral("value")));
        if (!date.isValid() || !value || values.contains(date)) {
            return {}; // A malformed/duplicate row must not become a plausible spend total.
        }
        values.insert(date, *value);
    }

    double total = 0.0;
    double maximum = 0.0;
    int reported = 0;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.key() < start || it.key() > end) {
            continue;
        }
        total += it.value();
        maximum = std::max(maximum, it.value());
        ++reported;
    }
    if (!std::isfinite(total)) {
        return {};
    }

    QVariantList points;
    for (int day = 0; day < days; ++day) {
        const QDate date = start.addDays(day);
        const QString label = date.toString(Qt::ISODate);
        const auto it = values.constFind(date);
        const bool known = it != values.cend();
        const double value = known ? it.value() : 0.0;
        points.append(
            QVariantMap{{QStringLiteral("date"), label},
                        {QStringLiteral("amount"), known ? QVariant(value) : QVariant{}},
                        {QStringLiteral("fraction"), maximum > 0.0 ? value / maximum : 0.0},
                        {QStringLiteral("description"),
                         tr("%1: %2").arg(label, known ? money(value) : tr("Not reported"))}});
    }
    const bool providerPartial = cost.value(QStringLiteral("historyPartial")).toBool();
    const bool partial = providerPartial || reported < days;
    QString summary = tr("No daily spend reported");
    if (reported > 0) {
        summary = tr("Reported spend: %1 · %2/%3 days reported")
                      .arg(money(total))
                      .arg(reported)
                      .arg(days);
    }
    return {{QStringLiteral("available"), true},
            {QStringLiteral("points"), points},
            {QStringLiteral("rangeLabel"),
             tr("%1 – %2 (UTC)").arg(start.toString(Qt::ISODate), end.toString(Qt::ISODate))},
            {QStringLiteral("summary"), summary},
            {QStringLiteral("partial"), partial},
            {QStringLiteral("estimated"), cost.value(QStringLiteral("historyEstimated")).toBool()},
            {QStringLiteral("includesCurrentDay"),
             cost.value(QStringLiteral("historyIncludesCurrentDay")).toBool()}};
}

} // namespace Kodometer
