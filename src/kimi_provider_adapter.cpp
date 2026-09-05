#include <kodometer/kimi_provider_adapter.hpp>

#include <kodometer/kimi_usage_parser.hpp>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QSysInfo>

#include <algorithm>

#ifndef KODOMETER_VERSION
#define KODOMETER_VERSION "development"
#endif

namespace Kodometer {
namespace {

QMap<QString, QString> credentialEnvironment()
{
    const QProcessEnvironment process = QProcessEnvironment::systemEnvironment();
    QMap<QString, QString> environment;
    // GCOVR_EXCL_BR_START -- process environment depends on the Plasma session
    for (const QString &key :
         {QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("KIMI_CODE_HOME")}) {
        if (process.contains(key)) {
            environment.insert(key, process.value(key));
        }
    }
    // GCOVR_EXCL_BR_STOP
    return environment;
}

QString asciiHeader(QString value)
{
    // GCOVR_EXCL_BR_START -- platform identity and Qt character iteration branches
    QString result;
    result.reserve(value.size());
    for (const QChar character : value) {
        const ushort code = character.unicode();
        if (code >= 0x20 && code <= 0x7e) {
            result.append(character);
        }
    }
    result = result.trimmed();
    const QString header = result.isEmpty() ? QStringLiteral("unknown") : result;
    // GCOVR_EXCL_BR_STOP
    return header;
}

void addCliIdentity(QNetworkRequest &request, const KimiCredentials &credentials)
{
    const QString version = QStringLiteral(KODOMETER_VERSION);
    const QString osVersion = asciiHeader(QSysInfo::productVersion());
    request.setRawHeader("User-Agent", QStringLiteral("Kodometer/%1").arg(version).toUtf8());
    request.setRawHeader("X-Msh-Platform", "kimi_code_cli");
    request.setRawHeader("X-Msh-Version", version.toUtf8());
    request.setRawHeader("X-Msh-Device-Name", asciiHeader(QSysInfo::machineHostName()).toUtf8());
    request.setRawHeader(
        "X-Msh-Device-Model",
        asciiHeader(
            QStringLiteral("Linux %1 %2").arg(osVersion, QSysInfo::currentCpuArchitecture()))
            .toUtf8());
    request.setRawHeader("X-Msh-Os-Version", osVersion.toUtf8());
    request.setRawHeader("X-Msh-Device-Id", credentials.deviceId.toUtf8());
}

} // namespace

// GCOVR_EXCL_BR_START -- Qt ownership and allocation branches
KimiProviderAdapter::KimiProviderAdapter(QNetworkAccessManager *network, QObject *parent)
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

QString KimiProviderAdapter::providerId() const
{
    return QStringLiteral("kimi");
}

bool KimiProviderAdapter::busy() const noexcept
{
    return m_busy;
}

QString KimiProviderAdapter::error() const
{
    return m_error;
}

QVariantMap KimiProviderAdapter::provider() const
{
    return m_provider;
}

void KimiProviderAdapter::setBaseEndpoint(const QUrl &endpoint)
{
    m_baseEndpoint = endpoint;
}

void KimiProviderAdapter::setEnvironment(const QMap<QString, QString> &environment)
{
    m_environment = environment;
}

void KimiProviderAdapter::setCurrentDateTime(const QDateTime &dateTime)
{
    m_currentDateTime = dateTime;
}

void KimiProviderAdapter::setTimeoutMilliseconds(int milliseconds)
{
    m_timeoutMilliseconds = std::max(milliseconds, 1);
}

void KimiProviderAdapter::refresh()
{
    if (m_busy) {
        return;
    }
    setBusy(true);
    setError({});
    QString credentialError;
    const auto credentials =
        KimiCredentialStore::resolve(environmentWithCredentialOverrides(m_environment), {}, {},
                                     currentDateTime(), &credentialError);
    if (!credentials) {
        completeFailure(credentialError);
        return;
    }
    m_credentials = *credentials;
    m_triedCliFallback = false;
    requestUsage();
}

QDateTime KimiProviderAdapter::currentDateTime() const
{
    return m_currentDateTime.isValid() ? m_currentDateTime.toUTC()
                                       : QDateTime::currentDateTimeUtc(); // GCOVR_EXCL_BR_LINE
}

QUrl KimiProviderAdapter::usageEndpoint() const
{
    QUrl result = m_baseEndpoint;
    QString path = result.path();
    // GCOVR_EXCL_BR_START -- all normalization routes are tested; QString adds branches
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    if (path.endsWith(QStringLiteral("/coding/v1"))) {
        path += QStringLiteral("/usages");
    }
    else if (path.endsWith(QStringLiteral("/coding"))) {
        path += QStringLiteral("/v1/usages");
    }
    else {
        path += QStringLiteral("/coding/v1/usages");
    }
    // GCOVR_EXCL_BR_STOP
    result.setPath(path, QUrl::DecodedMode);
    result.setQuery({});
    result.setFragment({});
    return result;
}

void KimiProviderAdapter::requestUsage()
{
    QNetworkRequest request(usageEndpoint());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_credentials.accessToken.toUtf8());
    if (m_credentials.source == KimiCredentialSource::Cli) {
        addCliIdentity(request, m_credentials);
    }

    m_responseData.clear();
    m_timedOut = false;
    m_responseTooLarge = false;
    m_reply = m_network->get(request);
    connect(m_reply, &QIODevice::readyRead, this, &KimiProviderAdapter::readReplyData);
    connect(m_reply, &QNetworkReply::finished, this, &KimiProviderAdapter::finishReply);
    m_timeout.start(m_timeoutMilliseconds);
}

bool KimiProviderAdapter::tryCliFallback()
{
    if (m_credentials.source != KimiCredentialSource::ApiKey ||
        m_triedCliFallback) { // GCOVR_EXCL_BR_LINE -- API and CLI rejection paths are tested
        return false;
    }
    QMap<QString, QString> environment = environmentWithCredentialOverrides(m_environment);
    environment.remove(QStringLiteral("KIMI_CODE_API_KEY"));
    const auto credentials = KimiCredentialStore::resolve(environment, {}, {}, currentDateTime());
    if (!credentials || credentials->source != KimiCredentialSource::Cli) { // GCOVR_EXCL_BR_LINE
        return false;
    }
    m_credentials = *credentials;
    m_triedCliFallback = true;
    requestUsage();
    return true;
}

void KimiProviderAdapter::readReplyData()
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

void KimiProviderAdapter::finishReply()
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
        completeFailure(QStringLiteral("Kimi Code usage response exceeds the 1 MiB limit"));
        return;
    }
    if (m_timedOut) {
        completeFailure(QStringLiteral("Kimi Code usage request timed out"));
        return;
    }
    if (statusCode == 400) {
        completeFailure(QStringLiteral("Kimi Code usage request was rejected"));
        return;
    }
    if (statusCode == 401) {
        if (tryCliFallback()) {
            return;
        }
        const QString message =
            m_credentials.source == KimiCredentialSource::Cli
                ? QStringLiteral("Kimi Code CLI credential is invalid or expired; sign in again or "
                                 "set KIMI_CODE_API_KEY")
                : QStringLiteral("Kimi Code rejected the API key");
        completeFailure(message);
        return;
    }
    if (statusCode == 403) {
        completeFailure(QStringLiteral("Kimi Code denied access to usage quota"));
        return;
    }
    if (statusCode == 429) {
        completeFailure(QStringLiteral("Kimi Code usage request was rate limited"));
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("Kimi Code usage request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("Kimi Code usage request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QString parseError;
    const auto provider = KimiUsageParser::parse(m_responseData, m_credentials.source,
                                                 currentDateTime(), &parseError);
    if (!provider) {
        completeFailure(parseError);
        return;
    }
    completeSuccess(*provider);
}

void KimiProviderAdapter::completeSuccess(const QVariantMap &provider)
{
    m_provider = provider;
    emit providerChanged();
    emit refreshSucceeded(m_provider);
    setBusy(false);
    emit refreshFinished(true);
}

void KimiProviderAdapter::completeFailure(const QString &message)
{
    setError(message);
    emit refreshFailed(message);
    setBusy(false);
    emit refreshFinished(false);
}

void KimiProviderAdapter::setBusy(bool busy)
{
    if (m_busy == busy) { // GCOVR_EXCL_LINE -- callers only transition state
        return;           // GCOVR_EXCL_LINE
    }
    m_busy = busy;
    emit busyChanged();
}

void KimiProviderAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
