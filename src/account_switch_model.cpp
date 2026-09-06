#include <kodometer/account_switch_model.hpp>
#include <kodometer/oauth_profiles.hpp>

namespace Kodometer {
namespace {
const QList<QPair<QString, QString>> Providers{
    {QStringLiteral("codex"), QStringLiteral("Codex")},
    {QStringLiteral("claude"), QStringLiteral("Claude")},
    {QStringLiteral("gemini"), QStringLiteral("Gemini")},
    {QStringLiteral("xai"), QStringLiteral("xAI")},
    {QStringLiteral("kimi"), QStringLiteral("Kimi Code")},
    {QStringLiteral("deepseek"), QStringLiteral("DeepSeek")},
    {QStringLiteral("zai"), QStringLiteral("z.ai")},
    {QStringLiteral("openrouter"), QStringLiteral("OpenRouter")}};

QVariantMap choice(const QString &id, const QString &name, bool available)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("available"), available}};
}
} // namespace

WalletAccounts *AccountSwitchModel::accounts() const
{
    return m_accounts.data();
}

void AccountSwitchModel::setAccounts(WalletAccounts *accounts)
{
    if (m_accounts == accounts)
        return;
    if (m_accounts)
        disconnect(m_accounts, nullptr, this, nullptr);
    m_accounts = accounts;
    if (m_accounts) {
        connect(m_accounts, &WalletAccounts::changed, this, &AccountSwitchModel::changed);
        connect(m_accounts, &QObject::destroyed, this, &AccountSwitchModel::changed);
    }
    emit changed();
}

QVariantList AccountSwitchModel::providers() const
{
    OAuthProfiles profiles;
    profiles.setConfiguration(m_oauthConfiguration);
    QVariantList result;
    for (const auto &[id, name] : Providers) {
        if (m_disabledProviders.contains(id))
            continue;
        const bool oauth = OAuthProfiles::supportsProvider(id);
        const bool enabled = !oauth || profiles.valid();
        QString selected;
        QVariantList entries;
        if (oauth) {
            selected = profiles.selectedId(id);
            entries = profiles.entries(id);
        }
        else {
            selected = m_walletSelections.value(id).toString();
            entries.append(choice({}, tr("Default (existing credentials)"), true));
            if (m_accounts && m_accounts->ready())
                entries.append(m_accounts->entries(id));
        }
        QVariantList choices;
        bool found = false;
        for (const QVariant &entry : entries) {
            const QVariantMap row = entry.toMap();
            const QString value = row.value(QStringLiteral("id")).toString();
            choices.append(choice(value, row.value(QStringLiteral("name")).toString(), true));
            found = found || value == selected;
        }
        if (!found)
            choices.append(choice(selected,
                                  oauth ? tr("Profile configuration unavailable")
                                        : tr("Selected account unavailable"),
                                  false));
        result.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("kind"), oauth ? QStringLiteral("profile") : QStringLiteral("account")},
            {QStringLiteral("selectedId"), selected},
            {QStringLiteral("enabled"), enabled},
            {QStringLiteral("choices"), choices}});
    }
    return result;
}

QVariantMap AccountSwitchModel::selectionChange(const QString &provider, const QString &id) const
{
    const QVariantList views = providers(); // Revalidate against current wallet availability.
    for (const QVariant &value : views) {
        const QVariantMap view = value.toMap();
        const QString candidateProvider = view.value(QStringLiteral("id")).toString();
        const bool enabled = view.value(QStringLiteral("enabled")).toBool();
        if (candidateProvider != provider || !enabled)
            continue;
        const QVariantList choices = view.value(QStringLiteral("choices")).toList();
        for (const QVariant &entry : choices) {
            const QVariantMap row = entry.toMap();
            const QString candidateId = row.value(QStringLiteral("id")).toString();
            const bool available = row.value(QStringLiteral("available")).toBool();
            if (candidateId != id || !available)
                continue;
            if (OAuthProfiles::supportsProvider(provider)) {
                QString configuration = m_oauthConfiguration;
                if (view.value(QStringLiteral("selectedId")).toString() != id) {
                    OAuthProfiles draft;
                    draft.setConfiguration(configuration);
                    (void)draft.selectProfile(provider, id); // The validated choice exists above.
                    configuration = draft.configuration();
                }
                return {{QStringLiteral("key"), QStringLiteral("oauthProfiles")},
                        {QStringLiteral("value"), configuration}};
            }
            return {{QStringLiteral("key"), provider + QStringLiteral("AccountId")},
                    {QStringLiteral("value"), id}};
        }
    }
    return {};
}
} // namespace Kodometer
