#include <kodometer/openai_api_adapter.hpp>
#include <kodometer/openai_costs.hpp>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QTimeZone>
#include <QUrlQuery>
#include <algorithm>

namespace Kodometer {
OpenAIApiAdapter::OpenAIApiAdapter(QNetworkAccessManager *network, QObject *parent)
    : ProviderAdapter(parent), m_network(network)
{
    if (m_network == nullptr)
        m_network = new QNetworkAccessManager(this);
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_reply) {
            m_timedOut = true;
            m_reply->abort();
        }
    });
}
QString OpenAIApiAdapter::providerId() const
{
    return QStringLiteral("openai-api");
}
QMap<QString, QString> OpenAIApiAdapter::environment() const
{
    QMap<QString, QString> values;
    const auto env = QProcessEnvironment::systemEnvironment();
    const auto key = QStringLiteral("OPENAI_ADMIN_KEY");
    if (env.contains(key))
        values.insert(key, env.value(key));
    return environmentWithCredentialOverrides(values);
}
QByteArray OpenAIApiAdapter::defaultCredentialContext() const
{
    return credentialContext(environment(), {u"OPENAI_ADMIN_KEY"});
}
void OpenAIApiAdapter::setEndpoint(const QUrl &endpoint)
{
    m_endpoint = endpoint;
}
void OpenAIApiAdapter::setTimeoutMilliseconds(int milliseconds)
{
    m_timeoutMilliseconds = std::max(1, milliseconds);
}
void OpenAIApiAdapter::refresh()
{
    if (m_reply)
        return;
    const QString key = environment().value(QStringLiteral("OPENAI_ADMIN_KEY")).trimmed();
    if (key.isEmpty()) {
        QVariantMap provider;
        provider.insert(QStringLiteral("id"), providerId());
        provider.insert(QStringLiteral("configured"), false);
        emit refreshSucceeded(provider);
        return;
    }
    if (key.size() > 65536 || std::any_of(key.cbegin(), key.cend(), [](QChar character) {
            return character.unicode() < 33 || character.unicode() > 126;
        })) {
        emit refreshFailed(QStringLiteral(
            "OpenAI Admin API key contains invalid characters or exceeds the 64 KiB limit"));
        return;
    }
    m_now = QDateTime::currentDateTimeUtc();
    const QDate today = m_now.date();
    // Cover the current calendar month and at least the last 30 UTC days.
    const QDate first(today.year(), today.month(), 1);
    const QDate start = std::min(first, today.addDays(-29));
    m_start = QDateTime(start, QTime(), QTimeZone::UTC).toSecsSinceEpoch();
    QUrl endpoint = m_endpoint;
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("start_time"), QString::number(m_start));
    query.addQueryItem(QStringLiteral("end_time"), QString::number(m_now.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("bucket_width"), QStringLiteral("1d"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("62"));
    endpoint.setQuery(query);
    QNetworkRequest request(endpoint);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Authorization", QByteArray("Bearer ") + key.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", QByteArray("Kodometer/") + KODOMETER_VERSION);
    m_data.clear();
    m_timedOut = false;
    m_oversized = false;
    m_reply = m_network->get(request);
    connect(m_reply, &QIODevice::readyRead, this, &OpenAIApiAdapter::readData);
    connect(m_reply, &QNetworkReply::finished, this, &OpenAIApiAdapter::finish);
    m_timeout.start(m_timeoutMilliseconds);
}
void OpenAIApiAdapter::readData()
{
    constexpr qsizetype maximum = 1024 * 1024;
    if (!m_reply || m_oversized)
        return;
    m_data.append(m_reply->read(maximum + 1 - m_data.size()));
    if (m_data.size() > maximum) {
        m_oversized = true;
        m_reply->abort();
    }
}
void OpenAIApiAdapter::finish()
{
    readData();
    m_timeout.stop();
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto networkError = m_reply->error();
    m_reply->deleteLater();
    m_reply = nullptr;
    QString error;
    if (m_oversized)
        error = QStringLiteral("OpenAI API spending response exceeds the 1 MiB limit");
    else if (m_timedOut)
        error = QStringLiteral("OpenAI API spending request timed out");
    else if (status < 200 || status >= 300 || networkError != QNetworkReply::NoError)
        error = QStringLiteral("OpenAI API spending request failed (HTTP %1)").arg(status);
    else {
        auto result = OpenAICosts::parse(m_data, m_start, m_now);
        if (result) {
            result->insert(QStringLiteral("id"), providerId());
            result->insert(QStringLiteral("configured"), true);
            emit refreshSucceeded(*result);
            return;
        }
        error = QStringLiteral("OpenAI API spending response is invalid or incomplete");
    }
    emit refreshFailed(error);
}
} // namespace Kodometer
