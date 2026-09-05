#include <kodometer/zai_provider_adapter.hpp>

#include <kodometer/zai_usage_parser.hpp>

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QTime>
#include <QTimeZone>
#include <QUrlQuery>

#include <algorithm>

namespace Kodometer {
namespace {

// GCOVR_EXCL_BR_START -- process environment and Qt container allocation branches
QMap<QString, QString> credentialEnvironment()
{
    const QProcessEnvironment process = QProcessEnvironment::systemEnvironment();
    QMap<QString, QString> environment;
    for (const QString &key :
         {QStringLiteral("Z_AI_API_KEY"), QStringLiteral("Z_AI_REGION"),
          QStringLiteral("Z_AI_USAGE_SCOPE"), QStringLiteral("Z_AI_BIGMODEL_ORGANIZATION"),
          QStringLiteral("Z_AI_BIGMODEL_PROJECT"), QStringLiteral("Z_AI_ORGANIZATION"),
          QStringLiteral("Z_AI_PROJECT"), QStringLiteral("BIGMODEL_API_KEY"),
          QStringLiteral("ZHIPU_API_KEY"), QStringLiteral("ZHIPUAI_API_KEY"),
          QStringLiteral("GLM_API_KEY")}) {
        if (process.contains(key)) {
            environment.insert(key, process.value(key));
        }
    }
    return environment;
}
// GCOVR_EXCL_BR_STOP

} // namespace

// GCOVR_EXCL_BR_START -- Qt ownership and allocation branches
ZaiProviderAdapter::ZaiProviderAdapter(QNetworkAccessManager *network, QObject *parent)
    : ProviderAdapter(parent),
      m_network(network == nullptr ? new QNetworkAccessManager(this) : network),
      m_environment(credentialEnvironment()), m_homeDirectory(QDir::homePath())
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

QString ZaiProviderAdapter::providerId() const
{
    return QStringLiteral("zai");
}

bool ZaiProviderAdapter::busy() const noexcept
{
    return m_busy;
}

QString ZaiProviderAdapter::error() const
{
    return m_error;
}

QVariantMap ZaiProviderAdapter::provider() const
{
    return m_provider;
}

void ZaiProviderAdapter::setEnvironment(const QMap<QString, QString> &environment)
{
    m_environment = environment;
}

void ZaiProviderAdapter::setHomeDirectory(const QString &homeDirectory)
{
    m_homeDirectory = homeDirectory;
}

void ZaiProviderAdapter::setQuotaEndpoint(const QUrl &endpoint)
{
    m_quotaEndpoint = endpoint;
}

void ZaiProviderAdapter::setModelUsageEndpoint(const QUrl &endpoint)
{
    m_modelUsageEndpoint = endpoint;
}

void ZaiProviderAdapter::setBalanceEndpoint(const QUrl &endpoint)
{
    m_balanceEndpoint = endpoint;
}

void ZaiProviderAdapter::setCurrentDateTime(const QDateTime &dateTime)
{
    m_currentDateTime = dateTime;
}

void ZaiProviderAdapter::setTimeoutMilliseconds(int milliseconds)
{
    m_timeoutMilliseconds = std::max(milliseconds, 1);
}

void ZaiProviderAdapter::refresh()
{
    if (m_busy) {
        return;
    }
    setBusy(true);
    setError({});
    m_pendingProvider.clear();

    QString credentialError;
    const auto credentials =
        ZaiCredentialResolver::resolve(m_environment, m_homeDirectory, &credentialError);
    if (!credentials) {
        completeFailure(credentialError);
        return;
    }
    m_credentials = *credentials;
    requestQuota();
}

QDateTime ZaiProviderAdapter::currentDateTime() const
{
    return m_currentDateTime.isValid() ? m_currentDateTime.toUTC()
                                       : QDateTime::currentDateTimeUtc(); // GCOVR_EXCL_BR_LINE
}

// GCOVR_EXCL_BR_START -- fixed URL construction carries Qt allocation branches
QUrl ZaiProviderAdapter::productionQuotaEndpoint() const
{
    return m_credentials.region == ZaiRegion::BigModelChina
               ? QUrl(QStringLiteral("https://open.bigmodel.cn/api/monitor/usage/quota/limit"))
               : QUrl(QStringLiteral("https://api.z.ai/api/monitor/usage/quota/limit"));
}

QUrl ZaiProviderAdapter::productionModelUsageEndpoint() const
{
    return m_credentials.region == ZaiRegion::BigModelChina
               ? QUrl(QStringLiteral("https://open.bigmodel.cn/api/monitor/usage/model-usage"))
               : QUrl(QStringLiteral("https://api.z.ai/api/monitor/usage/model-usage"));
}

QUrl ZaiProviderAdapter::productionBalanceEndpoint() const
{
    return QUrl(
        QStringLiteral("https://www.bigmodel.cn/api/biz/account/query-customer-account-report"));
}

// GCOVR_EXCL_BR_STOP

QNetworkRequest ZaiProviderAdapter::requestFor(const QUrl &url, bool teamHeaders) const
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", QByteArray("Bearer ") + m_credentials.apiKey.toUtf8());
    if (teamHeaders && m_credentials.scope == ZaiUsageScope::Team) {
        request.setRawHeader("Bigmodel-Organization", m_credentials.organizationId.toUtf8());
        request.setRawHeader("Bigmodel-Project", m_credentials.projectId.toUtf8());
    }
    return request;
}

QUrl ZaiProviderAdapter::modelUsageUrl(int daysBack) const
{
    // GCOVR_EXCL_BR_START -- test endpoint selection uses Qt value branches
    QUrl url =
        m_modelUsageEndpoint.isEmpty() ? productionModelUsageEndpoint() : m_modelUsageEndpoint;
    // GCOVR_EXCL_BR_STOP
    const QDateTime now = currentDateTime();
    const QDateTime start(now.date().addDays(-std::max(daysBack, 1)), QTime(0, 0), QTimeZone::UTC);
    const QDateTime end(now.date(), QTime(now.time().hour(), 59, 59), QTimeZone::UTC);
    const auto stamp = [](const QDateTime &value) {
        return value.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    };
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("startTime"), stamp(start));
    query.addQueryItem(QStringLiteral("endTime"), stamp(end));
    if (m_credentials.scope == ZaiUsageScope::Team) {
        query.addQueryItem(QStringLiteral("type"), QStringLiteral("3"));
    }
    url.setQuery(query);
    url.setFragment({});
    return url;
}

void ZaiProviderAdapter::requestQuota()
{
    // GCOVR_EXCL_BR_START -- test endpoint selection uses Qt value branches
    QUrl url = m_quotaEndpoint.isEmpty() ? productionQuotaEndpoint() : m_quotaEndpoint;
    // GCOVR_EXCL_BR_STOP
    if (m_credentials.scope == ZaiUsageScope::Team) {
        QUrlQuery query(url);
        query.removeAllQueryItems(QStringLiteral("type"));
        query.addQueryItem(QStringLiteral("type"), QStringLiteral("2"));
        url.setQuery(query);
    }
    watchReply(m_network->get(requestFor(url, true)), RequestKind::Quota);
}

void ZaiProviderAdapter::requestModelUsage(RequestKind kind, int daysBack)
{
    watchReply(m_network->get(requestFor(modelUsageUrl(daysBack), true)), kind);
}

void ZaiProviderAdapter::requestBalance()
{
    // GCOVR_EXCL_BR_START -- test endpoint selection uses Qt value branches
    const QUrl url = m_balanceEndpoint.isEmpty() ? productionBalanceEndpoint() : m_balanceEndpoint;
    // GCOVR_EXCL_BR_STOP
    watchReply(m_network->get(requestFor(url, false)), RequestKind::Balance);
}

void ZaiProviderAdapter::watchReply(QNetworkReply *reply, RequestKind kind)
{
    m_reply = reply;
    m_requestKind = kind;
    m_responseData.clear();
    m_timedOut = false;
    m_responseTooLarge = false;
    connect(reply, &QIODevice::readyRead, this, &ZaiProviderAdapter::readReplyData);
    connect(reply, &QNetworkReply::finished, this, &ZaiProviderAdapter::finishReply);
    // GCOVR_EXCL_BR_START -- both timeout choices are exercised; std::min adds branches
    const int timeout = kind == RequestKind::Balance ? std::min(m_timeoutMilliseconds, 5'000)
                                                     : m_timeoutMilliseconds;
    // GCOVR_EXCL_BR_STOP
    m_timeout.start(timeout);
}

void ZaiProviderAdapter::readReplyData()
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

void ZaiProviderAdapter::finishReply()
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

    if (kind != RequestKind::Quota) {
        // GCOVR_EXCL_BR_START -- all optional response classes and request kinds are tested
        if (!m_responseTooLarge && !m_timedOut && statusCode >= 200 && statusCode < 300) {
            if (kind == RequestKind::HourlyUsage || kind == RequestKind::DailyUsage) {
                const auto usage = ZaiUsageParser::parseModelUsage(m_responseData);
                if (usage) {
                    ZaiUsageParser::appendModelUsage(m_pendingProvider, *usage,
                                                     kind == RequestKind::HourlyUsage
                                                         ? QStringLiteral("Hourly tokens")
                                                         : QStringLiteral("Daily tokens"));
                }
            }
            else if (kind == RequestKind::Balance) {
                const auto balance = ZaiUsageParser::parseBalance(m_responseData);
                if (balance) {
                    ZaiUsageParser::appendBalance(m_pendingProvider, *balance);
                }
            }
        }
        // GCOVR_EXCL_BR_STOP
        advanceOptionalRequest(kind);
        return;
    }

    if (m_responseTooLarge) {
        completeFailure(QStringLiteral("z.ai quota response exceeds the 1 MiB limit"));
        return;
    }
    if (m_timedOut) {
        completeFailure(QStringLiteral("z.ai quota request timed out"));
        return;
    }
    if (statusCode == 401 || statusCode == 403) {
        completeFailure(QStringLiteral("z.ai rejected the API key"));
        return;
    }
    if (statusCode == 400) {
        completeFailure(QStringLiteral("z.ai quota request was rejected"));
        return;
    }
    if (statusCode == 429) {
        completeFailure(QStringLiteral("z.ai quota request was rate limited"));
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("z.ai quota request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("z.ai quota request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QString parseError;
    const auto provider = ZaiUsageParser::parseQuota(
        m_responseData, m_credentials.region, m_credentials.scope, currentDateTime(), &parseError);
    if (!provider) {
        completeFailure(parseError);
        return;
    }
    m_pendingProvider = *provider;
    requestModelUsage(RequestKind::HourlyUsage, 1);
}

void ZaiProviderAdapter::advanceOptionalRequest(RequestKind completedKind)
{
    if (completedKind == RequestKind::HourlyUsage) {
        requestModelUsage(RequestKind::DailyUsage, 30);
        return;
    }
    if (completedKind == RequestKind::DailyUsage &&
        m_credentials.region == ZaiRegion::BigModelChina) {
        requestBalance();
        return;
    }
    completeSuccess();
}

void ZaiProviderAdapter::completeSuccess()
{
    m_provider = m_pendingProvider;
    emit providerChanged();
    emit refreshSucceeded(m_provider);
    setBusy(false);
    emit refreshFinished(true);
}

void ZaiProviderAdapter::completeFailure(const QString &message)
{
    setError(message);
    emit refreshFailed(message);
    setBusy(false);
    emit refreshFinished(false);
}

void ZaiProviderAdapter::setBusy(bool busy)
{
    if (m_busy == busy) { // GCOVR_EXCL_LINE -- callers only transition state
        return;           // GCOVR_EXCL_LINE
    }
    m_busy = busy;
    emit busyChanged();
}

void ZaiProviderAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
