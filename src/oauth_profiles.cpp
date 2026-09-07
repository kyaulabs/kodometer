#include <kodometer/oauth_profiles.hpp>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace Kodometer {
namespace {

const QStringList ProviderIds{QStringLiteral("codex"), QStringLiteral("claude"),
                              QStringLiteral("gemini")};

QString selectionKey(const QString &provider)
{
    if (provider == QLatin1String("gemini"))
        return QStringLiteral("selectedGemini");
    return provider == QLatin1String("codex") ? QStringLiteral("selectedCodex")
                                              : QStringLiteral("selectedClaude");
}

bool controlCharacters(const QString &text)
{
    return std::any_of(text.cbegin(), text.cend(), [](QChar character) {
        return character.category() == QChar::Other_Control ||
               character.category() == QChar::Other_Format;
    });
}

QString encoded(const QJsonObject &document)
{
    return QString::fromUtf8(QJsonDocument(document).toJson(QJsonDocument::Compact));
}

} // namespace

OAuthProfiles::OAuthProfiles(QObject *parent) : QObject(parent) {}

QString OAuthProfiles::configuration() const
{
    return m_configuration;
}
bool OAuthProfiles::valid() const noexcept
{
    return m_valid;
}
QString OAuthProfiles::error() const
{
    return m_error;
}
bool OAuthProfiles::supportsProvider(const QString &provider)
{
    return ProviderIds.contains(provider);
}

QVariantMap OAuthProfiles::providers() const
{
    QVariantMap result;
    for (const QString &provider : ProviderIds) {
        result.insert(provider, QVariantMap{{QStringLiteral("entries"), entries(provider)},
                                            {QStringLiteral("selectedId"), selectedId(provider)}});
    }
    return result;
}

QVariantList OAuthProfiles::entries(const QString &provider) const
{
    if (!m_valid || !supportsProvider(provider)) {
        return {};
    }
    QVariantList result{QVariantMap{{QStringLiteral("id"), QStringLiteral("default")},
                                    {QStringLiteral("name"), tr("Default (environment)")},
                                    {QStringLiteral("directory"), QString{}}}};
    result.append(m_document.value(provider).toArray().toVariantList());
    return result;
}

QString OAuthProfiles::selectedId(const QString &provider) const
{
    return m_document.value(selectionKey(provider)).toString(QStringLiteral("default"));
}

QVariantMap OAuthProfiles::selected(const QString &provider) const
{
    const QString id = selectedId(provider);
    const QVariantList choices = entries(provider);
    for (const QVariant &entry : choices) {
        const QVariantMap row = entry.toMap();
        if (row.value(QStringLiteral("id")).toString() == id) {
            return row;
        }
    }
    return {};
}

QString OAuthProfiles::selectedName(const QString &provider) const
{
    return selected(provider).value(QStringLiteral("name")).toString();
}

QString OAuthProfiles::selectedDirectory(const QString &provider) const
{
    return selected(provider).value(QStringLiteral("directory")).toString();
}

QString OAuthProfiles::contextKey(const QString &provider) const
{
    if (!supportsProvider(provider)) {
        return {};
    }
    if (!m_valid) {
        return QStringLiteral("invalid");
    }
    return selectedId(provider) + QLatin1Char('\n') + selectedDirectory(provider);
}

std::optional<QJsonObject> OAuthProfiles::parse(const QString &configuration)
{
    const QByteArray bytes = configuration.toUtf8();
    if (bytes.size() > 65536) {
        return std::nullopt;
    }
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    const QStringList keys{QStringLiteral("version"),        QStringLiteral("codex"),
                           QStringLiteral("claude"),         QStringLiteral("selectedCodex"),
                           QStringLiteral("selectedClaude"), QStringLiteral("gemini"),
                           QStringLiteral("selectedGemini")};
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        if (!keys.contains(it.key())) {
            return std::nullopt;
        }
    }
    const QJsonValue version = root.value(QStringLiteral("version"));
    if (!version.isUndefined() && version.toDouble(-1.0) != 1.0) {
        return std::nullopt;
    }
    QJsonObject normalized{{QStringLiteral("version"), 1}};
    for (const QString &provider : ProviderIds) {
        const QJsonValue list = root.value(provider);
        if (!list.isUndefined() && !list.isArray()) {
            return std::nullopt;
        }
        const QJsonArray rows = list.toArray();
        if (rows.size() > 8) {
            return std::nullopt;
        }
        QSet<QString> ids{QStringLiteral("default")};
        QSet<QString> names;
        QSet<QString> directories;
        QJsonArray profiles;
        for (const QJsonValue &value : rows) {
            const QJsonObject row = value.toObject();
            const QJsonValue idValue = row.value(QStringLiteral("id"));
            const QJsonValue nameValue = row.value(QStringLiteral("name"));
            const QJsonValue directoryValue = row.value(QStringLiteral("directory"));
            if (row.size() != 3 || !idValue.isString() || !nameValue.isString() ||
                !directoryValue.isString()) {
                return std::nullopt;
            }
            const QString id = idValue.toString();
            const QString name = nameValue.toString().trimmed();
            const QString directory = directoryValue.toString();
            const QUuid uuid(id);
            const QString canonicalId = uuid.toString(QUuid::WithoutBraces);
            if (uuid.isNull() || canonicalId != id || ids.contains(id)) {
                return std::nullopt;
            }
            if (name.isEmpty() || name.size() > 64 || controlCharacters(name) ||
                names.contains(name)) {
                return std::nullopt;
            }
            if (directory.size() > 4096 || controlCharacters(directory) ||
                !QDir::isAbsolutePath(directory)) {
                return std::nullopt;
            }
            const QString clean = QDir::cleanPath(directory);
            if (directories.contains(clean)) {
                return std::nullopt;
            }
            ids.insert(id);
            names.insert(name);
            directories.insert(clean);
            profiles.append(QJsonObject{{QStringLiteral("id"), id},
                                        {QStringLiteral("name"), name},
                                        {QStringLiteral("directory"), clean}});
        }
        const QString key = selectionKey(provider);
        const QJsonValue selection = root.value(key);
        const QString id = selection.toString(QStringLiteral("default"));
        if ((!selection.isUndefined() && !selection.isString()) || !ids.contains(id)) {
            return std::nullopt;
        }
        normalized.insert(provider, profiles);
        normalized.insert(key, id);
    }
    return normalized;
}

void OAuthProfiles::setConfiguration(const QString &configuration)
{
    if (m_configuration == configuration) {
        return;
    }
    QMap<QString, QString> before;
    for (const QString &provider : ProviderIds) {
        before.insert(provider, contextKey(provider));
    }
    const auto parsed = parse(configuration);
    m_configuration = configuration;
    m_valid = parsed.has_value();
    m_document = parsed.value_or(QJsonObject{});
    setError(
        m_valid
            ? QString{}
            : tr("OAuth profile configuration is invalid; Codex, Claude, and Gemini are paused. "
                 "Restore Defaults or correct the profile settings."));
    for (const QString &provider : ProviderIds) {
        if (before.value(provider) != contextKey(provider)) {
            emit contextChanged(provider);
        }
    }
    emit configurationChanged();
}

bool OAuthProfiles::commit(const QJsonObject &document)
{
    const auto normalized = parse(encoded(document));
    if (!normalized) {
        setError(tr("Invalid profile: use a unique name (1–64 characters), an absolute folder path "
                    "(up to 4096 characters), and at most eight profiles per provider."));
        return false;
    }
    setConfiguration(encoded(*normalized));
    setError({});
    return true;
}

bool OAuthProfiles::addProfile(const QString &provider, const QString &name,
                               const QString &directory)
{
    if (!m_valid || !supportsProvider(provider)) {
        setError(tr("Choose a valid Codex, Claude, or Gemini profile configuration."));
        return false;
    }
    QJsonObject next = m_document;
    QJsonArray rows = next.value(provider).toArray();
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    rows.append(QJsonObject{{QStringLiteral("id"), id},
                            {QStringLiteral("name"), name},
                            {QStringLiteral("directory"), directory}});
    next.insert(provider, rows);
    next.insert(selectionKey(provider), id);
    return commit(next);
}

bool OAuthProfiles::selectProfile(const QString &provider, const QString &id)
{
    if (!m_valid || !supportsProvider(provider)) {
        setError(tr("Choose a valid Codex, Claude, or Gemini profile configuration."));
        return false;
    }
    const QVariantList choices = entries(provider);
    for (const QVariant &choice : choices) {
        if (choice.toMap().value(QStringLiteral("id")).toString() == id) {
            QJsonObject next = m_document;
            next.insert(selectionKey(provider), id);
            return commit(next);
        }
    }
    setError(tr("Select an existing profile."));
    return false;
}

bool OAuthProfiles::removeProfile(const QString &provider, const QString &id)
{
    if (!m_valid || !supportsProvider(provider) || id == QLatin1String("default")) {
        setError(tr("Only named Codex, Claude, or Gemini profiles can be removed."));
        return false;
    }
    QJsonObject next = m_document;
    QJsonArray rows = next.value(provider).toArray();
    for (qsizetype index = 0; index < rows.size(); ++index) {
        if (rows.at(index).toObject().value(QStringLiteral("id")).toString() == id) {
            rows.removeAt(index);
            next.insert(provider, rows);
            if (selectedId(provider) == id) {
                next.insert(selectionKey(provider), QStringLiteral("default"));
            }
            return commit(next); // Metadata only: never delete a credential file.
        }
    }
    setError(tr("Select an existing profile."));
    return false;
}

QString OAuthProfiles::localDirectory(const QUrl &url) const
{
    return url.isLocalFile() && url.host().isEmpty() ? url.toLocalFile() : QString{};
}

void OAuthProfiles::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
