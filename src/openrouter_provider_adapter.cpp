#include <kodometer/openrouter_provider_adapter.hpp>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QUrlQuery>

#include <algorithm>

namespace Kodometer {
namespace {

QMap<QString, QString> credentialEnvironment()
{
    // GCOVR_EXCL_BR_START -- process-environment iteration is covered through injected maps
    const QProcessEnvironment process = QProcessEnvironment::systemEnvironment();
    QMap<QString, QString> environment;
    for (const QString &key :
         {QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("OPENROUTER_MANAGEMENT_API_KEY"),
          QStringLiteral("OPENROUTER_HTTP_REFERER"), QStringLiteral("OPENROUTER_X_TITLE")}) {
        if (process.contains(key)) {
            environment.insert(key, process.value(key));
        }
    }
    return environment;
    // GCOVR_EXCL_BR_STOP
}

QString creditsAuthenticationError()
{
    return QStringLiteral("OpenRouter rejected the API key");
}

} // namespace

// GCOVR_EXCL_BR_START -- Qt ownership and allocation branches
OpenRouterProviderAdapter::OpenRouterProviderAdapter(QNetworkAccessManager *network,
                                                     QObject *parent)
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

QString OpenRouterProviderAdapter::providerId() const
{
    return QStringLiteral("openrouter");
}

bool OpenRouterProviderAdapter::busy() const noexcept
{
    return m_busy;
}

QString OpenRouterProviderAdapter::error() const
{
    return m_error;
}

QVariantMap OpenRouterProviderAdapter::provider() const
{
    return m_provider;
}

void OpenRouterProviderAdapter::setApiBaseEndpoint(const QUrl &endpoint)
{
    m_apiBaseEndpoint = endpoint;
}

void OpenRouterProviderAdapter::setActivityEndpoint(const QUrl &endpoint)
{
    m_activityEndpoint = endpoint;
}

void OpenRouterProviderAdapter::setEnvironment(const QMap<QString, QString> &environment)
{
    m_environment = environment;
}

void OpenRouterProviderAdapter::setCurrentDateTime(const QDateTime &dateTime)
{
    m_currentDateTime = dateTime;
}

void OpenRouterProviderAdapter::setTimeoutMilliseconds(int milliseconds)
{
    m_primaryTimeoutMilliseconds = std::max(milliseconds, 1);
    m_optionalTimeoutMilliseconds = m_primaryTimeoutMilliseconds;
}

void OpenRouterProviderAdapter::refresh()
{
    if (m_busy) {
        return;
    }
    setBusy(true);
    setError({});
    m_keyUsage.reset();
    m_activityHistory.reset();
    m_activityLatest.reset();
    m_keyDiagnostic.clear();
    m_activityDiagnostic.clear();

    QString credentialError;
    const auto credentials = OpenRouterCredentialResolver::resolve(
        environmentWithCredentialOverrides(m_environment), &credentialError);
    if (!credentials) {
        completeFailure(credentialError);
        return;
    }
    m_credentials = *credentials;
    if (m_credentials.managementApiKey.isEmpty()) {
        m_activityDiagnostic = QStringLiteral("Management API key not configured");
    }
    requestCredits();
}

QDateTime OpenRouterProviderAdapter::currentDateTime() const
{
    // GCOVR_EXCL_BR_START -- injected and system clocks share the same UTC conversion
    return m_currentDateTime.isValid() ? m_currentDateTime.toUTC()
                                       : QDateTime::currentDateTimeUtc();
    // GCOVR_EXCL_BR_STOP
}

QUrl OpenRouterProviderAdapter::apiEndpoint(const QString &resource) const
{
    // GCOVR_EXCL_BR_START -- endpoint output is asserted; Qt URL/string access adds branches
    QUrl result = m_apiBaseEndpoint;
    QString path = result.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    result.setPath(path + QLatin1Char('/') + resource, QUrl::DecodedMode);
    result.setQuery(QString{});
    result.setFragment({});
    return result;
    // GCOVR_EXCL_BR_STOP
}

QNetworkRequest OpenRouterProviderAdapter::requestFor(const QUrl &url, const QString &token,
                                                      bool applicationHeaders) const
{
    // GCOVR_EXCL_BR_START -- request headers are asserted; Qt request mutation adds branches
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    if (applicationHeaders) {
        request.setRawHeader("X-Title", m_credentials.clientTitle.toUtf8());
        if (!m_credentials.httpReferer.isEmpty()) {
            request.setRawHeader("HTTP-Referer", m_credentials.httpReferer.toUtf8());
        }
    }
    return request;
    // GCOVR_EXCL_BR_STOP
}

void OpenRouterProviderAdapter::requestCredits()
{
    watchReply(m_network->get(
                   requestFor(apiEndpoint(QStringLiteral("credits")), m_credentials.apiKey, true)),
               RequestKind::Credits);
}

void OpenRouterProviderAdapter::requestKey()
{
    watchReply(
        m_network->get(requestFor(apiEndpoint(QStringLiteral("key")), m_credentials.apiKey, false)),
        RequestKind::Key);
}

void OpenRouterProviderAdapter::requestActivityHistory()
{
    watchReply(
        m_network->get(requestFor(m_activityEndpoint, m_credentials.managementApiKey, false)),
        RequestKind::ActivityHistory);
}

void OpenRouterProviderAdapter::requestActivityLatest()
{
    QUrl url = m_activityEndpoint;
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("date"),
                       currentDateTime().date().addDays(-1).toString(Qt::ISODate));
    url.setQuery(query);
    watchReply(m_network->get(requestFor(url, m_credentials.managementApiKey, false)),
               RequestKind::ActivityLatest);
}

void OpenRouterProviderAdapter::watchReply(QNetworkReply *reply, RequestKind kind)
{
    m_reply = reply;
    m_requestKind = kind;
    m_responseData.clear();
    m_timedOut = false;
    m_responseTooLarge = false;
    connect(reply, &QIODevice::readyRead, this, &OpenRouterProviderAdapter::readReplyData);
    connect(reply, &QNetworkReply::finished, this, &OpenRouterProviderAdapter::finishReply);
    m_timeout.start(kind == RequestKind::Credits ? m_primaryTimeoutMilliseconds
                                                 : m_optionalTimeoutMilliseconds);
}

void OpenRouterProviderAdapter::readReplyData()
{
    // GCOVR_EXCL_BR_START -- active, oversized, and aborted reply states are exercised
    if (m_reply == nullptr || m_responseTooLarge || !m_reply->isOpen()) {
        return;
    }
    // GCOVR_EXCL_BR_STOP
    const qsizetype remaining = MaximumResponseSize + 1 - m_responseData.size();
    m_responseData.append(m_reply->read(remaining));
    if (m_responseData.size() > MaximumResponseSize) {
        m_responseTooLarge = true;
        m_reply->abort();
    }
}

void OpenRouterProviderAdapter::finishReply()
{
    if (m_reply == nullptr) { // GCOVR_EXCL_LINE -- only connected replies invoke this slot
        return;               // GCOVR_EXCL_LINE
    }
    readReplyData();
    m_timeout.stop();

    QNetworkReply *reply = m_reply;
    const RequestKind kind = m_requestKind;
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError networkError = reply->error();
    m_reply = nullptr;
    m_requestKind = RequestKind::None;
    reply->deleteLater();

    if (m_responseTooLarge) {
        if (kind == RequestKind::Credits) {
            completeFailure(QStringLiteral("OpenRouter credits response exceeds the 1 MiB limit"));
        }
        else if (kind == RequestKind::Key) {
            m_keyDiagnostic = QStringLiteral("Response exceeded the 1 MiB limit");
            // GCOVR_EXCL_BR_START -- optional failure routing is covered by normal request paths
            m_credentials.managementApiKey.isEmpty() ? completeSuccess() : requestActivityHistory();
            // GCOVR_EXCL_BR_STOP
        }
        else {
            m_activityHistory.reset();
            m_activityDiagnostic = QStringLiteral("Response exceeded the 1 MiB limit");
            completeSuccess();
        }
        return;
    }
    if (m_timedOut) {
        if (kind == RequestKind::Credits) {
            completeFailure(QStringLiteral("OpenRouter credits request timed out"));
        }
        else if (kind == RequestKind::Key) {
            m_keyDiagnostic = QStringLiteral("Request timed out");
            // GCOVR_EXCL_BR_START -- optional failure routing is covered by normal request paths
            m_credentials.managementApiKey.isEmpty() ? completeSuccess() : requestActivityHistory();
            // GCOVR_EXCL_BR_STOP
        }
        else {
            m_activityHistory.reset();
            m_activityDiagnostic = QStringLiteral("Request timed out");
            completeSuccess();
        }
        return;
    }
    if (kind == RequestKind::Credits) {
        finishCredits(statusCode, networkError);
    }
    else if (kind == RequestKind::Key) {
        finishKey(statusCode, networkError);
    }
    else { // GCOVR_EXCL_BR_LINE -- kind is never None here
        finishActivity(kind, statusCode, networkError);
    }
}

void OpenRouterProviderAdapter::finishCredits(int statusCode,
                                              QNetworkReply::NetworkError networkError)
{
    if (statusCode == 401 || statusCode == 403) {
        completeFailure(creditsAuthenticationError());
        return;
    }
    if (statusCode == 429) {
        completeFailure(QStringLiteral("OpenRouter credits request was rate limited"));
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("OpenRouter credits request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("OpenRouter credits request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }
    QString parseError;
    const auto credits = OpenRouterUsageParser::parseCredits(m_responseData, &parseError);
    if (!credits) {
        completeFailure(parseError);
        return;
    }
    m_credits = *credits;
    requestKey();
}

QString
OpenRouterProviderAdapter::optionalDiagnostic(int statusCode,
                                              QNetworkReply::NetworkError networkError) const
{
    // GCOVR_EXCL_BR_START -- HTTP and network diagnostics are asserted by adapter tests
    if (statusCode == 403) {
        return QStringLiteral("Management API key required");
    }
    if (statusCode > 0 && (statusCode < 200 || statusCode >= 300)) {
        return QStringLiteral("Request returned HTTP %1").arg(statusCode);
    }
    if (networkError != QNetworkReply::NoError) {
        return QStringLiteral("Request failed");
    }
    return {};
    // GCOVR_EXCL_BR_STOP
}

void OpenRouterProviderAdapter::finishKey(int statusCode, QNetworkReply::NetworkError networkError)
{
    // GCOVR_EXCL_BR_START -- success and rejection outcomes are covered
    if (statusCode < 200 || statusCode >= 300) {
        m_keyDiagnostic = optionalDiagnostic(statusCode, networkError);
    }
    else {
        QString parseError;
        // GCOVR_EXCL_BR_STOP
        m_keyUsage = OpenRouterUsageParser::parseKey(m_responseData, &parseError);
        if (!m_keyUsage) {
            m_keyDiagnostic = QStringLiteral("Response was invalid");
        }
    }
    if (m_credentials.managementApiKey.isEmpty()) {
        completeSuccess();
    }
    else {
        requestActivityHistory();
    }
}

void OpenRouterProviderAdapter::finishActivity(RequestKind kind, int statusCode,
                                               QNetworkReply::NetworkError networkError)
{
    // GCOVR_EXCL_BR_START -- optional request and parser outcomes are covered
    if (statusCode < 200 || statusCode >= 300) {
        m_activityHistory.reset();
        m_activityDiagnostic = optionalDiagnostic(statusCode, networkError);
        completeSuccess();
        return;
    }
    QString parseError;
    const auto activity =
        OpenRouterUsageParser::parseActivity(m_responseData, currentDateTime(), &parseError);
    if (!activity) {
        m_activityHistory.reset();
        m_activityDiagnostic = QStringLiteral("Response was invalid");
        completeSuccess();
        return;
    }
    // GCOVR_EXCL_BR_STOP
    if (kind == RequestKind::ActivityHistory) {
        m_activityHistory = activity;
        requestActivityLatest();
        return;
    }
    m_activityLatest = activity;
    const auto merged =
        OpenRouterUsageParser::mergeActivity(*m_activityHistory, *m_activityLatest, &parseError);
    if (!merged) { // GCOVR_EXCL_BR_LINE -- duplicate conflicts are tested directly
        m_activityHistory.reset();
        m_activityDiagnostic = QStringLiteral("Response was invalid");
    }
    else {
        m_activityHistory = merged;
    }
    completeSuccess();
}

void OpenRouterProviderAdapter::completeSuccess()
{
    m_provider =
        OpenRouterUsageParser::provider(m_credits, m_keyUsage, m_keyDiagnostic,
                                        m_activityDiagnostic, currentDateTime(), m_activityHistory);
    emit providerChanged();
    emit refreshSucceeded(m_provider);
    setBusy(false);
    emit refreshFinished(true);
}

void OpenRouterProviderAdapter::completeFailure(const QString &message)
{
    setError(message);
    emit refreshFailed(message);
    setBusy(false);
    emit refreshFinished(false);
}

void OpenRouterProviderAdapter::setBusy(bool busy)
{
    if (m_busy == busy) { // GCOVR_EXCL_LINE -- callers only transition state
        return;           // GCOVR_EXCL_LINE
    }
    m_busy = busy;
    emit busyChanged();
}

void OpenRouterProviderAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
