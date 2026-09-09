#include <kodometer/xai_provider_adapter.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QTime>
#include <QTimeZone>

#include <algorithm>

namespace Kodometer {
namespace {

QMap<QString, QString> credentialEnvironment()
{
    const QProcessEnvironment process = QProcessEnvironment::systemEnvironment();
    QMap<QString, QString> environment;
    for (const QString &key :
         {QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("XAI_TEAM_ID")}) {
        if (process.contains(key)) { // GCOVR_EXCL_BR_LINE
            environment.insert(key, process.value(key));
        }
    }
    return environment;
}

QNetworkRequest requestFor(const QUrl &endpoint, const XaiCredentials &credentials)
{
    QNetworkRequest request(endpoint);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + credentials.managementApiKey.toUtf8());
    return request;
}

QString authenticationError()
{
    return QStringLiteral("xAI rejected the Management API key");
}

} // namespace

// GCOVR_EXCL_BR_START -- Qt ownership and allocation branches
XaiProviderAdapter::XaiProviderAdapter(QNetworkAccessManager *network, QObject *parent)
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

QString XaiProviderAdapter::providerId() const
{
    return QStringLiteral("xai");
}

bool XaiProviderAdapter::busy() const noexcept
{
    return m_busy;
}

QString XaiProviderAdapter::error() const
{
    return m_error;
}

QVariantMap XaiProviderAdapter::provider() const
{
    return m_provider;
}

void XaiProviderAdapter::setBaseEndpoint(const QUrl &endpoint)
{
    m_baseEndpoint = endpoint;
}

QByteArray XaiProviderAdapter::defaultCredentialContext() const
{
    return credentialContext(m_environment, {u"XAI_MANAGEMENT_API_KEY", u"XAI_TEAM_ID"});
}

void XaiProviderAdapter::setEnvironment(const QMap<QString, QString> &environment)
{
    m_environment = environment;
}

void XaiProviderAdapter::setCurrentDateTime(const QDateTime &dateTime)
{
    m_currentDateTime = dateTime;
}

void XaiProviderAdapter::setTimeoutMilliseconds(int milliseconds)
{
    m_timeoutMilliseconds = std::max(milliseconds, 1);
}

void XaiProviderAdapter::refresh()
{
    if (m_busy) {
        return;
    }
    setBusy(true);
    m_refreshDateTime = currentDateTime();
    setError({});

    QString credentialError;
    auto environment = environmentWithCredentialOverrides(m_environment);
    const auto account = selectedAccountCredential();
    if (account) {
        environment.insert(QStringLiteral("XAI_MANAGEMENT_API_KEY"), *account);
        environment.insert(QStringLiteral("XAI_TEAM_ID"), selectedAccountTeamId());
    }
    const auto credentials = XaiCredentialResolver::resolve(environment, &credentialError);
    if (!credentials) {
        completeFailure(credentialError);
        return;
    }
    m_credentials = *credentials;
    requestBalance();
}

QUrl XaiProviderAdapter::endpoint(const QString &suffix) const
{
    QUrl result = m_baseEndpoint;
    QString path = result.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    path += QStringLiteral("/v1/billing/teams/") + m_credentials.teamId + suffix;
    result.setPath(path, QUrl::DecodedMode);
    result.setQuery({});
    result.setFragment({});
    return result;
}

QDateTime XaiProviderAdapter::currentDateTime() const
{
    return m_currentDateTime.isValid() ? m_currentDateTime.toUTC()
                                       : QDateTime::currentDateTimeUtc(); // GCOVR_EXCL_BR_LINE
}

void XaiProviderAdapter::requestBalance()
{
    QNetworkRequest request =
        requestFor(endpoint(QStringLiteral("/prepaid/balance")), m_credentials);
    watchReply(m_network->get(request), RequestKind::Balance);
}

void XaiProviderAdapter::requestUsage()
{
    QNetworkRequest request = requestFor(endpoint(QStringLiteral("/usage")), m_credentials);
    request.setRawHeader("Content-Type", "application/json");
    const QDateTime now = m_refreshDateTime;
    QDateTime start(now.date().addDays(-29), QTime(0, 0), QTimeZone::UTC);
    const auto formatted = [](const QDateTime &value) {
        return value.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    };
    // GCOVR_EXCL_BR_START -- Qt JSON allocation branches
    const QJsonObject timeRange{{QStringLiteral("startTime"), formatted(start)},
                                {QStringLiteral("endTime"), formatted(now)},
                                {QStringLiteral("timezone"), QStringLiteral("Etc/GMT")}};
    const QJsonObject analytics{
        {QStringLiteral("timeRange"), timeRange},
        {QStringLiteral("timeUnit"), QStringLiteral("TIME_UNIT_DAY")},
        {QStringLiteral("values"),
         QJsonArray{
             QJsonObject{{QStringLiteral("name"), QStringLiteral("usd")},
                         {QStringLiteral("aggregation"), QStringLiteral("AGGREGATION_SUM")}}}},
        {QStringLiteral("groupBy"), QJsonArray{}},
        {QStringLiteral("filters"), QJsonArray{}},
    };
    const QByteArray body =
        QJsonDocument(QJsonObject{{QStringLiteral("analyticsRequest"), analytics}})
            .toJson(QJsonDocument::Compact);
    // GCOVR_EXCL_BR_STOP
    watchReply(m_network->post(request, body), RequestKind::Usage);
}

void XaiProviderAdapter::watchReply(QNetworkReply *reply, RequestKind kind)
{
    m_reply = reply;
    m_requestKind = kind;
    m_responseData.clear();
    m_timedOut = false;
    m_responseTooLarge = false;
    connect(reply, &QIODevice::readyRead, this, &XaiProviderAdapter::readReplyData);
    connect(reply, &QNetworkReply::finished, this, &XaiProviderAdapter::finishReply);
    m_timeout.start(m_timeoutMilliseconds);
}

void XaiProviderAdapter::readReplyData()
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

void XaiProviderAdapter::finishReply()
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
        if (kind == RequestKind::Usage) {
            completeSuccess(std::nullopt);
        }
        else {
            completeFailure(QStringLiteral("xAI balance response exceeds the 1 MiB limit"));
        }
        return;
    }
    if (m_timedOut) {
        if (kind == RequestKind::Usage) {
            completeSuccess(std::nullopt);
        }
        else {
            completeFailure(QStringLiteral("xAI balance request timed out"));
        }
        return;
    }
    if (kind == RequestKind::Balance) {
        finishBalance(statusCode, networkError);
    }
    else if (kind == RequestKind::Usage) { // GCOVR_EXCL_BR_LINE -- kind is never None here
        finishUsage(statusCode);
    }
}

void XaiProviderAdapter::finishBalance(int statusCode, QNetworkReply::NetworkError networkError)
{
    if (statusCode == 401 || statusCode == 403) {
        completeFailure(authenticationError());
        return;
    }
    if (statusCode == 404) {
        completeFailure(QStringLiteral("xAI team was not found"));
        return;
    }
    if (statusCode == 429) {
        completeFailure(QStringLiteral("xAI Management API rate limit exceeded"));
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("xAI balance request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("xAI balance request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QString parseError;
    const auto balance = XaiUsageParser::parseBalance(m_responseData, &parseError);
    if (!balance) {
        completeFailure(parseError);
        return;
    }
    m_balance = *balance;
    requestUsage();
}

void XaiProviderAdapter::finishUsage(int statusCode)
{
    if (statusCode == 401 || statusCode == 403) {
        completeFailure(authenticationError());
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        completeSuccess(std::nullopt);
        return;
    }
    const auto history = XaiUsageParser::parseHistory(m_responseData);
    completeSuccess(history);
}

void XaiProviderAdapter::completeSuccess(const std::optional<XaiUsageHistory> &history)
{
    m_provider = XaiUsageParser::provider(m_balance, history, m_refreshDateTime);
    emit providerChanged();
    emit refreshSucceeded(m_provider);
    setBusy(false);
    emit refreshFinished(true);
}

void XaiProviderAdapter::completeFailure(const QString &message)
{
    setError(message);
    emit refreshFailed(message);
    setBusy(false);
    emit refreshFinished(false);
}

void XaiProviderAdapter::setBusy(bool busy)
{
    if (m_busy == busy) { // GCOVR_EXCL_LINE -- callers only transition state
        return;           // GCOVR_EXCL_LINE
    }
    m_busy = busy;
    emit busyChanged();
}

void XaiProviderAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
