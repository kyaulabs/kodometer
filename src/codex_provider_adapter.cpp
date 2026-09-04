#include <kodometer/codex_provider_adapter.hpp>

#include <kodometer/codex_usage_parser.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QProcessEnvironment>

#include <algorithm>

namespace Kodometer {
namespace {

constexpr auto ClientId = "app_EMoamEEZ73f0CkXaXp7hrann";

QString defaultCredentialPath()
{
    const QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
    QMap<QString, QString> environment;
    if (processEnvironment.contains(QStringLiteral("CODEX_HOME"))) {
        environment.insert(QStringLiteral("CODEX_HOME"),
                           processEnvironment.value(QStringLiteral("CODEX_HOME")));
    }
    return CodexCredentialStore::authenticationFilePath(environment);
}

QNetworkRequest requestFor(const QUrl &endpoint)
{
    QNetworkRequest request(endpoint);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "Kodometer/0.1.0");
    return request;
}

QString tokenString(const QJsonObject &object, const QString &key, const QString &fallback)
{
    const QString value = object.value(key).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

} // namespace

// GCOVR_EXCL_BR_START -- Qt ownership and allocation branches
CodexProviderAdapter::CodexProviderAdapter(QNetworkAccessManager *network, QObject *parent)
    : ProviderAdapter(parent),
      m_network(network == nullptr ? new QNetworkAccessManager(this) : network),
      m_credentialPath(defaultCredentialPath())
{
    // GCOVR_EXCL_BR_STOP
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_reply == nullptr) {
            return;
        }
        m_timedOut = true;
        m_reply->abort();
    });
}

QString CodexProviderAdapter::providerId() const
{
    return QStringLiteral("codex");
}

bool CodexProviderAdapter::busy() const noexcept
{
    return m_busy;
}

QString CodexProviderAdapter::error() const
{
    return m_error;
}

QVariantMap CodexProviderAdapter::provider() const
{
    return m_provider;
}

void CodexProviderAdapter::setCredentialPath(const QString &path)
{
    m_credentialPath = path;
}

void CodexProviderAdapter::setUsageEndpoint(const QUrl &endpoint)
{
    m_usageEndpoint = endpoint;
}

void CodexProviderAdapter::setTokenEndpoint(const QUrl &endpoint)
{
    m_tokenEndpoint = endpoint;
}

void CodexProviderAdapter::setTimeoutMilliseconds(int milliseconds)
{
    m_timeoutMilliseconds = std::max(milliseconds, 1);
}

void CodexProviderAdapter::refresh()
{
    if (m_busy) {
        return;
    }

    setBusy(true);
    setError({});
    m_refreshedDuringRequest = false;
    QString credentialError;
    const auto credentials = CodexCredentialStore::load(m_credentialPath, &credentialError);
    if (!credentials) {
        completeFailure(credentialError);
        return;
    }
    m_credentials = *credentials;

    if (m_credentials.needsRefresh()) {
        requestToken();
    }
    else {
        requestUsage();
    }
}

void CodexProviderAdapter::requestUsage()
{
    QNetworkRequest request = requestFor(m_usageEndpoint);
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_credentials.accessToken.toUtf8());
    if (!m_credentials.accountId.isEmpty()) {
        request.setRawHeader("ChatGPT-Account-Id", m_credentials.accountId.toUtf8());
    }
    watchReply(m_network->get(request), RequestKind::Usage);
}

void CodexProviderAdapter::requestToken()
{
    m_refreshedDuringRequest = true;
    QNetworkRequest request = requestFor(m_tokenEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    // GCOVR_EXCL_BR_START -- Qt container allocation branches
    const QJsonObject body{
        {QStringLiteral("client_id"), QString::fromLatin1(ClientId)},
        {QStringLiteral("grant_type"), QStringLiteral("refresh_token")},
        {QStringLiteral("refresh_token"), m_credentials.refreshToken},
        {QStringLiteral("scope"), QStringLiteral("openid profile email")},
    };
    // GCOVR_EXCL_BR_STOP
    watchReply(m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact)),
               RequestKind::Token);
}

void CodexProviderAdapter::watchReply(QNetworkReply *reply, RequestKind kind)
{
    m_reply = reply;
    m_requestKind = kind;
    m_responseData.clear();
    m_timedOut = false;
    m_responseTooLarge = false;
    connect(reply, &QIODevice::readyRead, this, &CodexProviderAdapter::readReplyData);
    connect(reply, &QNetworkReply::finished, this, &CodexProviderAdapter::finishReply);
    m_timeout.start(m_timeoutMilliseconds);
}

void CodexProviderAdapter::readReplyData()
{
    if (m_reply == nullptr || m_responseTooLarge) { // GCOVR_EXCL_BR_LINE
        return;
    }
    const qsizetype remaining = MaximumResponseSize + 1 - m_responseData.size();
    m_responseData.append(m_reply->read(remaining));
    if (m_responseData.size() > MaximumResponseSize) {
        m_responseTooLarge = true;
        m_reply->abort();
    }
}

void CodexProviderAdapter::finishReply()
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
        // GCOVR_EXCL_BR_START -- QString allocation branches
        completeFailure(kind == RequestKind::Usage
                            ? QStringLiteral("Codex usage response exceeds the 1 MiB limit")
                            : QStringLiteral("Codex token response exceeds the 1 MiB limit"));
        // GCOVR_EXCL_BR_STOP
        return;
    }
    if (m_timedOut) {
        // GCOVR_EXCL_BR_START -- QString allocation branches
        completeFailure(kind == RequestKind::Usage
                            ? QStringLiteral("Codex usage request timed out")
                            : QStringLiteral("Codex token request timed out"));
        // GCOVR_EXCL_BR_STOP
        return;
    }

    if (kind == RequestKind::Usage) {
        finishUsage(statusCode, networkError);
    }
    else if (kind == RequestKind::Token) { // GCOVR_EXCL_BR_LINE -- kind is never None here
        finishToken(statusCode, networkError);
    }
}

void CodexProviderAdapter::finishUsage(int statusCode, QNetworkReply::NetworkError networkError)
{
    if ((statusCode == 401 || statusCode == 403) && !m_refreshedDuringRequest &&
        !m_credentials.apiKey && !m_credentials.refreshToken.isEmpty()) {
        requestToken();
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("Codex usage request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("Codex usage request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QString parseError;
    const auto provider = CodexUsageParser::parse(m_responseData, m_credentials,
                                                  QDateTime::currentDateTimeUtc(), &parseError);
    if (!provider) {
        completeFailure(parseError);
        return;
    }
    completeSuccess(*provider);
}

void CodexProviderAdapter::finishToken(int statusCode, QNetworkReply::NetworkError networkError)
{
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("Codex token request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("Codex token request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(m_responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) { // GCOVR_EXCL_BR_LINE
        completeFailure(QStringLiteral("Codex token endpoint returned invalid JSON"));
        return;
    }
    const QJsonObject response = document.object();
    m_credentials.accessToken =
        tokenString(response, QStringLiteral("access_token"), m_credentials.accessToken);
    m_credentials.refreshToken =
        tokenString(response, QStringLiteral("refresh_token"), m_credentials.refreshToken);
    m_credentials.idToken =
        tokenString(response, QStringLiteral("id_token"), m_credentials.idToken);
    m_credentials.lastRefresh = QDateTime::currentDateTimeUtc();

    QString saveError;
    if (!CodexCredentialStore::save(
            m_credentialPath, m_credentials,
            &saveError)) {          // GCOVR_EXCL_LINE -- credential path changed concurrently
        completeFailure(saveError); // GCOVR_EXCL_LINE
        return;                     // GCOVR_EXCL_LINE
    }
    const auto reloaded = CodexCredentialStore::load(m_credentialPath, &saveError);
    if (!reloaded) {                // GCOVR_EXCL_LINE -- atomic save was replaced concurrently
        completeFailure(saveError); // GCOVR_EXCL_LINE
        return;                     // GCOVR_EXCL_LINE
    }
    m_credentials = *reloaded;
    requestUsage();
}

void CodexProviderAdapter::completeSuccess(const QVariantMap &provider)
{
    m_provider = provider;
    emit providerChanged();
    emit refreshSucceeded(m_provider);
    setBusy(false);
    emit refreshFinished(true);
}

void CodexProviderAdapter::completeFailure(const QString &message)
{
    setError(message);
    emit refreshFailed(message);
    setBusy(false);
    emit refreshFinished(false);
}

void CodexProviderAdapter::setBusy(bool busy)
{
    if (m_busy == busy) { // GCOVR_EXCL_LINE -- callers only transition state
        return;           // GCOVR_EXCL_LINE
    }
    m_busy = busy;
    emit busyChanged();
}

void CodexProviderAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
