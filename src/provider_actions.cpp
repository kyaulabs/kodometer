#include <kodometer/provider_actions.hpp>

#include <QDesktopServices>
#include <QVariantMap>

#include <array>
#include <utility>

namespace Kodometer {
namespace {

struct Destination
{
    const char *provider;
    const char *region;
    const char *dashboard;
    const char *documentation;
};

// No URL is constructed from provider payloads, account identifiers, or credentials.
constexpr std::array Destinations{
    Destination{"codex", "", "https://chatgpt.com/codex/settings/usage",
                "https://developers.openai.com/codex/"},
    Destination{"claude", "", "https://claude.ai/settings/usage",
                "https://code.claude.com/docs/en/overview"},
    Destination{"gemini", "", "https://console.cloud.google.com/",
                "https://cloud.google.com/gemini/docs/codeassist/overview"},
    Destination{"xai", "", "https://console.x.ai/", "https://docs.x.ai/"},
    Destination{"kimi", "", "https://www.kimi.com/code/console", "https://www.kimi.com/code/docs/"},
    Destination{"deepseek", "", "https://platform.deepseek.com/usage",
                "https://api-docs.deepseek.com/"},
    Destination{"openrouter", "", "https://openrouter.ai/activity",
                "https://openrouter.ai/docs/overview"},
    Destination{"zai", "global", "https://z.ai/manage-apikey/coding-plan/personal/my-plan",
                "https://docs.z.ai/"},
    Destination{"zai", "bigmodel-cn", "https://bigmodel.cn/", "https://docs.bigmodel.cn/"},
};

const Destination *destination(const QString &provider, const QString &region)
{
    for (const Destination &entry : Destinations) {
        if (provider == QLatin1String(entry.provider) &&
            (entry.region[0] == '\0' || region == QLatin1String(entry.region))) {
            return &entry;
        }
    }
    return nullptr;
}

QVariantMap action(const QString &id, const QString &label, const char *url)
{
    QVariantMap result;
    result.insert(QStringLiteral("id"), id);
    result.insert(QStringLiteral("label"), label);
    result.insert(QStringLiteral("url"), QUrl(QString::fromLatin1(url)));
    return result;
}

} // namespace

ProviderActions::ProviderActions(QObject *parent)
    : ProviderActions([](const QUrl &url) { return QDesktopServices::openUrl(url); }, parent)
{}

ProviderActions::ProviderActions(UrlOpener opener, QObject *parent)
    : QObject(parent), m_opener(std::move(opener))
{}

QString ProviderActions::providerId() const
{
    return m_providerId;
}

QString ProviderActions::region() const
{
    return m_region;
}

QString ProviderActions::error() const
{
    return m_error;
}

void ProviderActions::setProviderId(const QString &providerId)
{
    if (m_providerId == providerId) {
        return;
    }
    m_providerId = providerId;
    setError({});
    emit contextChanged();
}

void ProviderActions::setRegion(const QString &region)
{
    if (m_region == region) {
        return;
    }
    m_region = region;
    setError({});
    emit contextChanged();
}

QVariantList ProviderActions::actions() const
{
    const Destination *entry = destination(m_providerId, m_region);
    if (entry == nullptr) {
        return {};
    }
    QVariantList result;
    result.append(action(QStringLiteral("dashboard"), tr("Open dashboard"), entry->dashboard));
    result.append(
        action(QStringLiteral("documentation"), tr("Documentation"), entry->documentation));
    return result;
}

bool ProviderActions::open(const QString &actionId)
{
    const Destination *entry = destination(m_providerId, m_region);
    const char *url = nullptr;
    if (entry != nullptr) {
        if (actionId == QLatin1String("dashboard")) {
            url = entry->dashboard;
        }
        else if (actionId == QLatin1String("documentation")) {
            url = entry->documentation;
        }
    }
    if (url == nullptr) {
        setError(tr("This provider action is unavailable"));
        return false;
    }
    const QUrl target(QString::fromLatin1(url));
    if (!m_opener || !m_opener(target)) {
        setError(tr("Could not open the provider page in your browser"));
        return false;
    }
    setError({});
    return true;
}

void ProviderActions::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
