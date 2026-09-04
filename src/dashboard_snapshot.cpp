#include <codexbar/dashboard_snapshot.hpp>

#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

#include <cmath>
#include <limits>
#include <utility>

namespace CodexBar {
namespace {

std::optional<DashboardSnapshot> fail(QString message, QString *error)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
    return std::nullopt;
}

bool isInteger(const QJsonValue &value)
{
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    return std::floor(number) == number;
}

QString validateProvider(const QJsonValue &value, QSet<QString> &ids)
{
    if (!value.isObject()) {
        return QStringLiteral("Each providers entry must be an object");
    }

    const QJsonObject provider = value.toObject();
    const QJsonValue idValue = provider.value(QStringLiteral("id"));
    if (!idValue.isString()) {
        return QStringLiteral("Each provider requires a non-empty string id");
    }

    const QString id = idValue.toString();
    if (id.trimmed().isEmpty()) {
        return QStringLiteral("Each provider requires a non-empty string id");
    }
    if (ids.contains(id)) {
        return QStringLiteral("Provider id '%1' is duplicated").arg(id);
    }
    ids.insert(id);

    const QJsonValue nameValue = provider.value(QStringLiteral("name"));
    if (!nameValue.isString()) {
        return QStringLiteral("Provider '%1' requires a non-empty string name").arg(id);
    }
    if (nameValue.toString().trimmed().isEmpty()) {
        return QStringLiteral("Provider '%1' requires a non-empty string name").arg(id);
    }
    if (!provider.value(QStringLiteral("enabled")).isBool()) {
        return QStringLiteral("Provider '%1' requires a boolean enabled field").arg(id);
    }
    if (!provider.value(QStringLiteral("windows")).isArray()) {
        return QStringLiteral("Provider '%1' requires a windows array").arg(id);
    }

    return {};
}

} // namespace

DashboardSnapshot::DashboardSnapshot(QJsonObject document, QDateTime generatedAt,
                                     int staleAfterSeconds)
    : m_document(std::move(document)), m_generatedAt(std::move(generatedAt)),
      m_staleAfterSeconds(staleAfterSeconds)
{}

std::optional<DashboardSnapshot> DashboardSnapshot::fromJson(QByteArrayView payload, QString *error)
{
    if (payload.size() > MaximumPayloadBytes) {
        return fail(QStringLiteral("Dashboard snapshot is too large"), error);
    }
    if (payload.trimmed().isEmpty()) {
        return fail(QStringLiteral("Dashboard snapshot is empty"), error);
    }

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(payload.toByteArray(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return fail(QStringLiteral("Invalid JSON at byte %1: %2")
                        .arg(parseError.offset)
                        .arg(parseError.errorString()),
                    error);
    }
    if (!json.isObject()) {
        return fail(QStringLiteral("Dashboard snapshot root must be an object"), error);
    }

    const QJsonObject document = json.object();
    const QJsonValue schemaVersion = document.value(QStringLiteral("schemaVersion"));
    if (!isInteger(schemaVersion)) {
        return fail(QStringLiteral("schemaVersion must be an integer"), error);
    }
    if (schemaVersion.toInt() != SupportedSchemaVersion) {
        return fail(
            QStringLiteral("Dashboard schema version %1 is unsupported").arg(schemaVersion.toInt()),
            error);
    }

    const QJsonValue generatedAtValue = document.value(QStringLiteral("generatedAt"));
    if (!generatedAtValue.isString()) {
        return fail(QStringLiteral("generatedAt must be an ISO 8601 string"), error);
    }
    const QDateTime generatedAt = QDateTime::fromString(generatedAtValue.toString(), Qt::ISODate);
    if (!generatedAt.isValid()) {
        return fail(QStringLiteral("generatedAt must be a valid ISO 8601 timestamp"), error);
    }

    const QJsonValue staleAfter = document.value(QStringLiteral("staleAfterSeconds"));
    if (!isInteger(staleAfter) || staleAfter.toDouble() < 0 ||
        staleAfter.toDouble() > std::numeric_limits<int>::max()) {
        return fail(QStringLiteral("staleAfterSeconds must be a non-negative integer"), error);
    }

    const QJsonValue providersValue = document.value(QStringLiteral("providers"));
    if (!providersValue.isArray()) {
        return fail(QStringLiteral("providers must be an array"), error);
    }

    QSet<QString> ids;
    const QJsonArray providers = providersValue.toArray();
    for (const QJsonValue &provider : providers) {
        const QString providerError = validateProvider(provider, ids);
        if (!providerError.isEmpty()) {
            return fail(providerError, error);
        }
    }

    if (error != nullptr) {
        error->clear();
    }
    return DashboardSnapshot(document, generatedAt.toUTC(), staleAfter.toInt());
}

int DashboardSnapshot::schemaVersion() const noexcept
{
    return SupportedSchemaVersion;
}

QDateTime DashboardSnapshot::generatedAt() const
{
    return m_generatedAt;
}

int DashboardSnapshot::staleAfterSeconds() const noexcept
{
    return m_staleAfterSeconds;
}

QJsonArray DashboardSnapshot::providers() const
{
    return m_document.value(QStringLiteral("providers")).toArray();
}

std::optional<QJsonObject> DashboardSnapshot::provider(const QString &id) const
{
    for (const QJsonValue &entry : providers()) {
        const QJsonObject object = entry.toObject();
        if (object.value(QStringLiteral("id")).toString() == id) {
            return object;
        }
    }
    return std::nullopt;
}

QVariantMap DashboardSnapshot::toVariantMap() const
{
    return m_document.toVariantMap();
}

bool DashboardSnapshot::isStale(const QDateTime &now) const
{
    return m_generatedAt.addSecs(m_staleAfterSeconds) < now.toUTC();
}

} // namespace CodexBar
