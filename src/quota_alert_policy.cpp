#include <kodometer/quota_alert_policy.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>

namespace Kodometer {
namespace {

QString providerName(const QString &id)
{
    // Only these public names can reach the desktop; never use payload identity or labels.
    constexpr std::array names{
        std::pair{"codex", "Codex"},    std::pair{"claude", "Claude"},
        std::pair{"gemini", "Gemini"},  std::pair{"xai", "xAI"},
        std::pair{"kimi", "Kimi Code"}, std::pair{"deepseek", "DeepSeek"},
        std::pair{"zai", "z.ai"},       std::pair{"openrouter", "OpenRouter"}};
    for (const auto &[key, name] : names) {
        if (id == QLatin1String(key)) {
            return QString::fromLatin1(name);
        }
    }
    return {};
}

std::optional<double> remainingPercent(const QVariant &value)
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
    if (!std::isfinite(number) || number < 0.0 || number > 100.0) {
        return std::nullopt;
    }
    return number;
}

} // namespace

QuotaAlertPolicy::QuotaAlertPolicy(QObject *parent) : QObject(parent) {}

bool QuotaAlertPolicy::enabled() const noexcept
{
    return m_enabled;
}

int QuotaAlertPolicy::thresholdPercent() const noexcept
{
    return m_thresholdPercent;
}

void QuotaAlertPolicy::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    m_lowProviders.clear();
    emit settingsChanged();
}

void QuotaAlertPolicy::setThresholdPercent(int percent)
{
    const int bounded = std::clamp(percent, 1, 50);
    if (m_thresholdPercent == bounded) {
        return;
    }
    m_thresholdPercent = bounded;
    m_lowProviders.clear();
    emit settingsChanged();
}

void QuotaAlertPolicy::observe(const QVariantMap &provider)
{
    if (!m_enabled) {
        return;
    }
    const QString id = provider.value(QStringLiteral("id")).toString();
    const QString name = providerName(id);
    if (name.isEmpty()) {
        return;
    }
    std::optional<double> lowest;
    bool complete = true;
    const QVariantList windows = provider.value(QStringLiteral("windows")).toList();
    for (const QVariant &value : windows) {
        const QVariantMap window = value.toMap();
        if (window.value(QStringLiteral("idle")).toBool()) {
            continue;
        }
        const auto remaining = remainingPercent(window.value(QStringLiteral("remainingPercent")));
        if (!remaining) {
            complete = false;
            continue;
        }
        if (!lowest || *remaining < *lowest) {
            lowest = remaining;
        }
    }
    if (!lowest) {
        return; // Missing/invalid quota is not evidence of recovery.
    }
    if (complete && *lowest >= m_thresholdPercent + 5) {
        m_lowProviders.remove(id);
    }
    else if (*lowest <= m_thresholdPercent && !m_lowProviders.contains(id)) {
        m_lowProviders.insert(
            id); // Bound to the eight known IDs, not provider-supplied window keys.
        emit alertReady(
            tr("%1 quota is low").arg(name),
            tr("Most constrained quota: %1% remaining.").arg(QString::number(*lowest, 'f', 1)));
    }
}

} // namespace Kodometer
