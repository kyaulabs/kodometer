#include <kodometer/credential_store.hpp>
#include <kodometer/wallet_accounts.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace Kodometer {
namespace {
const QStringList Providers{QStringLiteral("deepseek"), QStringLiteral("kimi"),
                            QStringLiteral("openrouter"), QStringLiteral("xai")};

bool validKey(const QString &key)
{
    return key.size() <= CredentialStore::MaximumSecretSize &&
           std::all_of(key.cbegin(), key.cend(),
                       [](QChar ch) { return ch.unicode() >= 0x21 && ch.unicode() <= 0x7e; });
}

bool validTeam(const QString &provider, const QString &team)
{
    if (provider != QLatin1String("xai"))
        return team.isEmpty();
    static const QRegularExpression identifier(QStringLiteral("\\A[A-Za-z0-9_-]{1,256}\\z"));
    return identifier.match(team).hasMatch();
}

bool validId(const QString &id)
{
    const QUuid uuid(id);
    const QString canonical = uuid.toString(QUuid::WithoutBraces);
    return !uuid.isNull() && canonical == id;
}
} // namespace

WalletAccounts::WalletAccounts(CredentialBackend *backend, QObject *parent)
    : QObject(parent), m_backend(backend)
{}

bool WalletAccounts::ready() const noexcept
{
    return m_ready;
}
QString WalletAccounts::error() const
{
    return m_error;
}
bool WalletAccounts::supportsProvider(const QString &provider)
{
    return Providers.contains(provider);
}

bool WalletAccounts::isAccountEntry(const QString &entry)
{
    return entry.startsWith(QLatin1String("accounts/deepseek/")) ||
           entry.startsWith(QLatin1String("accounts/kimi/")) ||
           entry.startsWith(QLatin1String("accounts/openrouter/")) ||
           entry.startsWith(QLatin1String("accounts/xai/"));
}

QString WalletAccounts::entryName(const QString &provider, const QString &id)
{
    if (!supportsProvider(provider) || !validId(id))
        return {};
    return QStringLiteral("accounts/%1/%2").arg(provider, id);
}

QVariantMap WalletAccounts::providers() const
{
    QVariantMap result;
    for (const QString &provider : Providers)
        result.insert(provider, entries(provider));
    return result;
}

QVariantList WalletAccounts::entries(const QString &provider) const
{
    QMap<QString, QVariantMap> sorted;
    const QString prefix = QStringLiteral("accounts/%1/").arg(provider);
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        if (it.key().startsWith(prefix)) {
            sorted.insert(it->name, {{QStringLiteral("id"), it.key().mid(prefix.size())},
                                     {QStringLiteral("name"), it->name}});
        }
    }
    QVariantList result;
    for (const QVariantMap &entry : sorted)
        result.append(entry);
    return result;
}

QString WalletAccounts::name(const QString &provider, const QString &id) const
{
    return m_entries.value(entryName(provider, id)).name;
}

std::optional<QString> WalletAccounts::key(const QString &provider, const QString &id) const
{
    const auto it = m_entries.constFind(entryName(provider, id));
    if (!m_ready || it == m_entries.cend())
        return std::nullopt;
    return it->key;
}

QString WalletAccounts::managementKey(const QString &provider, const QString &id) const
{
    return m_entries.value(entryName(provider, id)).managementKey;
}

QString WalletAccounts::teamId(const QString &provider, const QString &id) const
{
    return m_entries.value(entryName(provider, id)).teamId;
}

std::optional<WalletAccounts::Account>
WalletAccounts::validated(const QString &provider, const QString &name, const QString &key,
                          const QString &managementKey, const QString &teamId)
{
    const QString label = name.trimmed();
    const QString secret = key.trimmed();
    const QString management = managementKey.trimmed();
    const QString team = teamId.trimmed();
    if (label.isEmpty() || label.size() > 64 || secret.isEmpty() || !validKey(secret) ||
        !validKey(management))
        return std::nullopt;
    const bool badName = std::any_of(label.cbegin(), label.cend(), [](QChar ch) {
        return ch.category() == QChar::Other_Control || ch.category() == QChar::Other_Format;
    });
    const bool unsupportedManagement =
        provider != QLatin1String("openrouter") && !management.isEmpty();
    if (badName || unsupportedManagement || !validTeam(provider, team))
        return std::nullopt;
    return Account{label, secret, management, team};
}

void WalletAccounts::setAvailable(bool available)
{
    m_available = available;
    if (available)
        (void)reload();
    else
        unavailable(tr("KWallet accounts are unavailable."));
}

bool WalletAccounts::reload()
{
    if (!m_available)
        return false;
    const quint64 revision = ++m_readRevision;
    QString error;
    const auto loaded = m_backend->readAccountEntries(&error);
    // KWallet calls can dispatch close/update signals while waiting for D-Bus replies.
    if (revision != m_readRevision) {
        return m_ready;
    }
    if (!loaded) {
        unavailable(tr("KWallet accounts could not be read."));
        return false;
    }
    QMap<QString, Account> parsed;
    QMap<QString, int> counts;
    QSet<QString> names;
    bool valid = loaded->size() <= 32;
    for (auto it = loaded->cbegin(); valid && it != loaded->cend(); ++it) {
        const QStringList parts = it.key().split(QLatin1Char('/'));
        if (parts.size() != 3 || parts.first() != QLatin1String("accounts")) {
            valid = false;
            break;
        }
        const QString provider = parts.at(1);
        if (entryName(provider, parts.at(2)) != it.key() || ++counts[provider] > 8) {
            valid = false;
            break;
        }
        const QByteArray payload = it.value().toUtf8();
        const qsizetype maximum = provider == QLatin1String("openrouter") ? 384 * 1024 : 256 * 1024;
        if (payload.size() > maximum) {
            valid = false;
            break;
        }
        const QJsonDocument document = QJsonDocument::fromJson(payload);
        const QJsonObject object = document.object();
        const QJsonValue label = object.value(QStringLiteral("name"));
        const QJsonValue key = object.value(QStringLiteral("key"));
        const QJsonValue management = object.value(QStringLiteral("managementKey"));
        const bool paired = provider == QLatin1String("openrouter") && !management.isUndefined();
        const bool validManagement = !paired || management.isString();
        const QJsonValue team = object.value(QStringLiteral("teamId"));
        const bool scoped = provider == QLatin1String("xai");
        const bool validTeamType = !scoped || team.isString();
        const int fieldCount = 2 + (paired ? 1 : 0) + (scoped ? 1 : 0);
        if (object.size() != fieldCount || !label.isString() || !key.isString() ||
            !validManagement || !validTeamType) {
            valid = false;
            break;
        }
        const auto account = validated(provider, label.toString(), key.toString(),
                                       management.toString(), team.toString());
        if (!account) {
            valid = false;
            break;
        }
        const QString nameKey = provider + QLatin1Char('/') + account->name;
        if (names.contains(nameKey)) {
            valid = false;
            break;
        }
        names.insert(nameKey);
        parsed.insert(it.key(), *account);
    }
    if (!valid) {
        unavailable(
            tr("KWallet account data is invalid. Repair the named entries in KWallet Manager."));
        return false;
    }
    const bool different = !m_ready || m_entries != parsed || !m_error.isEmpty();
    m_entries = parsed;
    m_ready = true;
    m_error.clear();
    if (different)
        emit changed();
    return true;
}

bool WalletAccounts::editable()
{
    if (!m_available) {
        setError(tr("KWallet is not ready."));
        return false;
    }
    return reload(); // Do not edit from an obsolete cached account list.
}

QString WalletAccounts::addAccount(const QString &provider, const QString &name, const QString &key,
                                   const QString &managementKey, const QString &teamId)
{
    if (!supportsProvider(provider)) {
        setError(tr("Choose DeepSeek, Kimi Code, OpenRouter, or xAI."));
        return {};
    }
    if (!editable())
        return {};
    const auto account = validated(provider, name, key, managementKey, teamId);
    if (!account) {
        setError(tr("Use a name of 1–64 characters and a required API key. An optional Management "
                    "key is supported only for OpenRouter. Keys must be printable ASCII without "
                    "internal whitespace, at most 64 KiB each. xAI also requires a team ID of "
                    "1–256 ASCII letters, digits, underscores, or hyphens."));
        return {};
    }
    const QVariantList existing = entries(provider);
    if (existing.size() >= 8) {
        setError(tr("At most eight accounts are supported per provider."));
        return {};
    }
    for (const QVariant &entry : existing) {
        if (entry.toMap().value(QStringLiteral("name")).toString() == account->name) {
            setError(tr("Account names must be unique within a provider."));
            return {};
        }
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return write(entryName(provider, id), *account) ? id : QString{};
}

bool WalletAccounts::replaceAccount(const QString &provider, const QString &id, const QString &key,
                                    const QString &managementKey, const QString &teamId)
{
    if (!editable())
        return false;
    const QString entry = entryName(provider, id);
    const auto it = m_entries.constFind(entry);
    if (it == m_entries.cend()) {
        setError(tr("Select an available named account."));
        return false;
    }
    const auto replacement = validated(provider, it->name, key, managementKey, teamId);
    if (!replacement) {
        setError(
            tr("Enter the required API key and, optionally, an OpenRouter Management key. Keys "
               "must be printable ASCII without internal whitespace, at most 64 KiB each. xAI also "
               "requires a team ID of 1–256 ASCII letters, digits, underscores, or hyphens."));
        return false;
    }
    return write(entry, *replacement);
}

bool WalletAccounts::removeAccount(const QString &provider, const QString &id)
{
    if (!editable())
        return false;
    const QString entry = entryName(provider, id);
    if (!m_entries.contains(entry)) {
        setError(tr("Select an available named account."));
        return false;
    }
    QString error;
    if (!m_backend->removeSecret(entry, &error)) {
        setError(tr("KWallet could not remove the account."));
        return false;
    }
    return reload();
}

bool WalletAccounts::write(const QString &entry, const Account &account)
{
    QJsonObject object{{QStringLiteral("name"), account.name},
                       {QStringLiteral("key"), account.key}};
    if (!account.managementKey.isEmpty()) {
        object.insert(QStringLiteral("managementKey"), account.managementKey);
    }
    if (!account.teamId.isEmpty()) {
        object.insert(QStringLiteral("teamId"), account.teamId);
    }
    const QString payload = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    QString error;
    if (!m_backend->writeSecret(entry, payload, &error)) {
        setError(tr("KWallet could not store the account."));
        return false;
    }
    return reload();
}

void WalletAccounts::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit changed();
}

void WalletAccounts::unavailable(const QString &error)
{
    ++m_readRevision;
    const bool different = m_ready || !m_entries.isEmpty() || m_error != error;
    m_ready = false;
    m_entries.clear();
    m_error = error;
    if (different)
        emit changed();
}
} // namespace Kodometer
