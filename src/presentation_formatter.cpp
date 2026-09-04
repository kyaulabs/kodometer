#include <codexbar/presentation_formatter.hpp>

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace CodexBar {

double PresentationFormatter::clampPercent(double percent) noexcept
{
    return std::clamp(percent, 0.0, 100.0);
}

QString PresentationFormatter::remainingLabel(const QVariant &remainingPercent)
{
    if (!remainingPercent.isValid() || remainingPercent.isNull()) {
        return QStringLiteral("—");
    }

    bool validNumber = false;
    const double value = remainingPercent.toDouble(&validNumber);
    if (!validNumber || !std::isfinite(value)) {
        return QStringLiteral("—");
    }
    return QStringLiteral("%1% left").arg(compactNumber(clampPercent(value)));
}

QString PresentationFormatter::resetLabel(const QString &timestamp)
{
    return resetLabelAt(timestamp, QDateTime::currentDateTimeUtc());
}

QString PresentationFormatter::resetLabelAt(const QString &timestamp, const QDateTime &now)
{
    const QDateTime reset = QDateTime::fromString(timestamp, Qt::ISODate);
    if (!reset.isValid() || !now.isValid()) {
        return {};
    }

    const qint64 seconds = now.toUTC().secsTo(reset.toUTC());
    if (seconds <= 0) {
        return QStringLiteral("now");
    }
    if (seconds < 60) {
        return QStringLiteral("in %1s").arg(seconds);
    }
    if (seconds < 3'600) {
        return QStringLiteral("in %1m").arg(seconds / 60);
    }
    if (seconds < 86'400) {
        const qint64 hours = seconds / 3'600;
        const qint64 minutes = (seconds % 3'600) / 60;
        if (minutes == 0) {
            return QStringLiteral("in %1h").arg(hours);
        }
        return QStringLiteral("in %1h %2m").arg(hours).arg(minutes);
    }

    const qint64 days = seconds / 86'400;
    const qint64 hours = (seconds % 86'400) / 3'600;
    if (hours == 0) {
        return QStringLiteral("in %1d").arg(days);
    }
    return QStringLiteral("in %1d %2h").arg(days).arg(hours);
}

QString PresentationFormatter::windowTitle(const QString &kind, const QString &label)
{
    if (!label.trimmed().isEmpty()) {
        return label;
    }
    if (kind == QStringLiteral("session")) {
        return QStringLiteral("Session");
    }
    if (kind == QStringLiteral("weekly")) {
        return QStringLiteral("Weekly");
    }
    if (kind == QStringLiteral("tertiary")) {
        return QStringLiteral("Monthly");
    }
    if (kind.isEmpty()) {
        return QStringLiteral("Usage");
    }

    QString title = kind;
    title.replace(QRegularExpression(QStringLiteral("[-_]+")), QStringLiteral(" "));
    title = title.toLower();
    title[0] = title.at(0).toUpper();
    return title;
}

QString PresentationFormatter::creditsLabel(double amount, const QString &unit)
{
    if (unit.compare(QStringLiteral("USD"), Qt::CaseInsensitive) == 0) {
        return usdLabel(amount);
    }
    return QStringLiteral("%1 %2").arg(compactNumber(amount), unit);
}

QString PresentationFormatter::usdLabel(double amount)
{
    return QStringLiteral("$%1").arg(amount, 0, 'f', 2);
}

QString PresentationFormatter::compactNumber(double amount)
{
    QString result = QString::number(amount, 'f', 2);
    while (result.endsWith(QLatin1Char('0'))) {
        result.chop(1);
    }
    if (result.endsWith(QLatin1Char('.'))) {
        result.chop(1);
    }
    return result;
}

} // namespace CodexBar
