#include <kodometer/deepseek_provider_adapter.hpp>

#include <kodometer/deepseek_credentials.hpp>
#include <kodometer/deepseek_usage_parser.hpp>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QProcessEnvironment>

#include <algorithm>

namespace Kodometer {
namespace {

QMap<QString, QString> credentialEnvironment()
{
    const QProcessEnvironment process = QProcessEnvironment::systemEnvironment();
    QMap<QString, QString> environment;
    for (const QString &key :
         {QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("DEEPSEEK_KEY")}) {
        if (process.contains(key)) { // GCOVR_EXCL_BR_LINE
            environment.insert(key, process.value(key));
        }
    }
    return environment;
}

} // namespace

// GCOVR_EXCL_BR_START -- Qt ownership and allocation branches
DeepSeekProviderAdapter::DeepSeekProviderAdapter(QNetworkAccessManager *network, QObject *parent)
    : ProviderAdapter(parent),
      m_network(network == nullptr ? new QNetworkAccessManager(this) : network),
      m_environment(credentialEnvironment())
{
    // GCOVR_EXCL_BR_STOP
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_reply == nullptr) { // GCOVR_EXCL_LINE -- an active timer always has a reply
            return;               // GCOVR_EXCL_LINE
        }
        m_timedOut = true;
        m_reply->abort();
    });
}

QString DeepSeekProviderAdapter::providerId() const
{
    return QStringLiteral("deepseek");
}

bool DeepSeekProviderAdapter::busy() const noexcept
{
    return m_busy;
}

QString DeepSeekProviderAdapter::error() const
{
    return m_error;
}

QVariantMap DeepSeekProviderAdapter::provider() const
{
    return m_provider;
}

void DeepSeekProviderAdapter::setBaseEndpoint(const QUrl &endpoint)
{
    m_baseEndpoint = endpoint;
}

void DeepSeekProviderAdapter::setEnvironment(const QMap<QString, QString> &environment)
{
    m_environment = environment;
}

void DeepSeekProviderAdapter::setCurrentDateTime(const QDateTime &dateTime)
{
    m_currentDateTime = dateTime;
}

void DeepSeekProviderAdapter::setTimeoutMilliseconds(int milliseconds)
{
    m_timeoutMilliseconds = std::max(milliseconds, 1);
}

void DeepSeekProviderAdapter::refresh()
{
    if (m_busy) {
        return;
    }
    setBusy(true);
    setError({});

    QString credentialError;
    const auto credentials = DeepSeekCredentialResolver::resolve(
        environmentWithCredentialOverrides(m_environment), &credentialError);
    if (!credentials) {
        completeFailure(credentialError);
        return;
    }

    QNetworkRequest request(balanceEndpoint());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", QByteArray("Bearer ") + credentials->apiKey.toUtf8());

    m_responseData.clear();
    m_timedOut = false;
    m_responseTooLarge = false;
    m_reply = m_network->get(request);
    connect(m_reply, &QIODevice::readyRead, this, &DeepSeekProviderAdapter::readReplyData);
    connect(m_reply, &QNetworkReply::finished, this, &DeepSeekProviderAdapter::finishReply);
    m_timeout.start(m_timeoutMilliseconds);
}

QUrl DeepSeekProviderAdapter::balanceEndpoint() const
{
    QUrl result = m_baseEndpoint;
    QString path = result.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    path += QStringLiteral("/user/balance");
    result.setPath(path);
    result.setQuery({});
    result.setFragment({});
    return result;
}

QDateTime DeepSeekProviderAdapter::currentDateTime() const
{
    return m_currentDateTime.isValid() ? m_currentDateTime.toUTC()
                                       : QDateTime::currentDateTimeUtc(); // GCOVR_EXCL_BR_LINE
}

void DeepSeekProviderAdapter::readReplyData()
{
    if (m_reply == nullptr || m_responseTooLarge ||
        !m_reply->isOpen()) { // GCOVR_EXCL_BR_LINE -- aborted replies are already drained
        return;
    }
    const qsizetype remaining = MaximumResponseSize + 1 - m_responseData.size();
    m_responseData.append(m_reply->read(remaining));
    if (m_responseData.size() > MaximumResponseSize) {
        m_responseTooLarge = true;
        m_reply->abort();
    }
}

void DeepSeekProviderAdapter::finishReply()
{
    if (m_reply == nullptr) { // GCOVR_EXCL_LINE -- only connected replies invoke this slot
        return;               // GCOVR_EXCL_LINE
    }
    readReplyData();
    m_timeout.stop();

    QNetworkReply *reply = m_reply;
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError networkError = reply->error();
    m_reply = nullptr;
    reply->deleteLater();

    if (m_responseTooLarge) {
        completeFailure(QStringLiteral("DeepSeek balance response exceeds the 1 MiB limit"));
        return;
    }
    if (m_timedOut) {
        completeFailure(QStringLiteral("DeepSeek balance request timed out"));
        return;
    }
    if (statusCode == 401 || statusCode == 403) {
        completeFailure(QStringLiteral("DeepSeek rejected the API key"));
        return;
    }
    if (statusCode == 400) {
        completeFailure(QStringLiteral("DeepSeek balance request was rejected"));
        return;
    }
    if (statusCode == 429) {
        completeFailure(QStringLiteral("DeepSeek balance request was rate limited"));
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("DeepSeek balance request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("DeepSeek balance request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QString parseError;
    const auto provider =
        DeepSeekUsageParser::parse(m_responseData, currentDateTime(), &parseError);
    if (!provider) {
        completeFailure(parseError);
        return;
    }
    m_provider = *provider;
    emit providerChanged();
    emit refreshSucceeded(m_provider);
    setBusy(false);
    emit refreshFinished(true);
}

void DeepSeekProviderAdapter::completeFailure(const QString &message)
{
    setError(message);
    emit refreshFailed(message);
    setBusy(false);
    emit refreshFinished(false);
}

void DeepSeekProviderAdapter::setBusy(bool busy)
{
    if (m_busy == busy) { // GCOVR_EXCL_LINE -- callers only transition state
        return;           // GCOVR_EXCL_LINE
    }
    m_busy = busy;
    emit busyChanged();
}

void DeepSeekProviderAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
