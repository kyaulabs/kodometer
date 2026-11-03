#include <kodometer/openai_provider_adapter.hpp>

#include <kodometer/openai_usage_parser.hpp>

#include <QDir>
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
    return OpenAICredentialStore::authenticationFilePath(environment);
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
OpenAIProviderAdapter::OpenAIProviderAdapter(QNetworkAccessManager *network, QObject *parent)
    : ProviderAdapter(parent),
      m_network(network == nullptr ? new QNetworkAccessManager(this) : network),
      m_credentialPath(defaultCredentialPath()), m_defaultCredentialPath(m_credentialPath)
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

QString OpenAIProviderAdapter::providerId() const
{
    return QStringLiteral("openai");
}

bool OpenAIProviderAdapter::busy() const noexcept
{
    return m_busy;
}

QString OpenAIProviderAdapter::error() const
{
    return m_error;
}

QVariantMap OpenAIProviderAdapter::provider() const
{
    return m_provider;
}

void OpenAIProviderAdapter::setCredentialPath(const QString &path)
{
    m_credentialPath = path;
    m_defaultCredentialPath = path;
}

void OpenAIProviderAdapter::setProfileDirectory(const QString &directory)
{
    if (directory.isEmpty()) {
        m_credentialPath = m_defaultCredentialPath;
        return;
    }
    m_credentialPath = QDir(directory).filePath(QStringLiteral("auth.json"));
}

void OpenAIProviderAdapter::setUsageEndpoint(const QUrl &endpoint)
{
    m_usageEndpoint = endpoint;
}

void OpenAIProviderAdapter::setTokenEndpoint(const QUrl &endpoint)
{
    m_tokenEndpoint = endpoint;
}

void OpenAIProviderAdapter::setTimeoutMilliseconds(int milliseconds)
{
    m_timeoutMilliseconds = std::max(milliseconds, 1);
}

void OpenAIProviderAdapter::refresh()
{
    if (m_busy) {
        return;
    }

    setBusy(true);
    setError({});
    m_refreshedDuringRequest = false;
    QString credentialError;
    m_requestCredentialPath = m_credentialPath;
    const auto credentials = OpenAICredentialStore::load(m_requestCredentialPath, &credentialError);
    if (!credentials) {
        completeFailure(credentialError);
        return;
    }
    m_credentials = *credentials;
    setHistoryIdentity((m_credentials.accountId.isEmpty()
                            ? (m_credentials.refreshToken.isEmpty() ? m_credentials.accessToken
                                                                    : m_credentials.refreshToken)
                            : m_credentials.accountId)
                           .toUtf8());

    if (m_credentials.needsRefresh()) {
        requestToken();
    }
    else {
        requestUsage();
    }
}

void OpenAIProviderAdapter::requestUsage()
{
    QNetworkRequest request = requestFor(m_usageEndpoint);
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_credentials.accessToken.toUtf8());
    if (!m_credentials.accountId.isEmpty()) {
        request.setRawHeader("ChatGPT-Account-Id", m_credentials.accountId.toUtf8());
    }
    watchReply(m_network->get(request), RequestKind::Usage);
}

void OpenAIProviderAdapter::requestToken()
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

void OpenAIProviderAdapter::watchReply(QNetworkReply *reply, RequestKind kind)
{
    m_reply = reply;
    m_requestKind = kind;
    m_responseData.clear();
    m_timedOut = false;
    m_responseTooLarge = false;
    connect(reply, &QIODevice::readyRead, this, &OpenAIProviderAdapter::readReplyData);
    connect(reply, &QNetworkReply::finished, this, &OpenAIProviderAdapter::finishReply);
    m_timeout.start(m_timeoutMilliseconds);
}

void OpenAIProviderAdapter::readReplyData()
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

void OpenAIProviderAdapter::finishReply()
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
                            ? QStringLiteral("OpenAI usage response exceeds the 1 MiB limit")
                            : QStringLiteral("OpenAI token response exceeds the 1 MiB limit"));
        // GCOVR_EXCL_BR_STOP
        return;
    }
    if (m_timedOut) {
        // GCOVR_EXCL_BR_START -- QString allocation branches
        completeFailure(kind == RequestKind::Usage
                            ? QStringLiteral("OpenAI usage request timed out")
                            : QStringLiteral("OpenAI token request timed out"));
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

void OpenAIProviderAdapter::finishUsage(int statusCode, QNetworkReply::NetworkError networkError)
{
    if ((statusCode == 401 || statusCode == 403) && !m_refreshedDuringRequest &&
        !m_credentials.apiKey && !m_credentials.refreshToken.isEmpty()) {
        requestToken();
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("OpenAI usage request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("OpenAI usage request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QString parseError;
    const auto provider = OpenAIUsageParser::parse(m_responseData, m_credentials,
                                                   QDateTime::currentDateTimeUtc(), &parseError);
    if (!provider) {
        completeFailure(parseError);
        return;
    }
    completeSuccess(*provider);
}

void OpenAIProviderAdapter::finishToken(int statusCode, QNetworkReply::NetworkError networkError)
{
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("OpenAI token request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("OpenAI token request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(m_responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) { // GCOVR_EXCL_BR_LINE
        completeFailure(QStringLiteral("OpenAI token endpoint returned invalid JSON"));
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
    if (!OpenAICredentialStore::save(
            m_requestCredentialPath, m_credentials,
            &saveError)) {          // GCOVR_EXCL_LINE -- credential path changed concurrently
        completeFailure(saveError); // GCOVR_EXCL_LINE
        return;                     // GCOVR_EXCL_LINE
    }
    const auto reloaded = OpenAICredentialStore::load(m_requestCredentialPath, &saveError);
    if (!reloaded) {                // GCOVR_EXCL_LINE -- atomic save was replaced concurrently
        completeFailure(saveError); // GCOVR_EXCL_LINE
        return;                     // GCOVR_EXCL_LINE
    }
    m_credentials = *reloaded;
    setHistoryIdentity(
        (m_credentials.accountId.isEmpty() ? m_credentials.refreshToken : m_credentials.accountId)
            .toUtf8(),
        m_credentials.accountId.isEmpty());
    requestUsage();
}

void OpenAIProviderAdapter::completeSuccess(const QVariantMap &provider)
{
    m_provider = provider;
    emit providerChanged();
    emit refreshSucceeded(m_provider);
    setBusy(false);
    emit refreshFinished(true);
}

void OpenAIProviderAdapter::completeFailure(const QString &message)
{
    setError(message);
    emit refreshFailed(message);
    setBusy(false);
    emit refreshFinished(false);
}

void OpenAIProviderAdapter::setBusy(bool busy)
{
    if (m_busy == busy) { // GCOVR_EXCL_LINE -- callers only transition state
        return;           // GCOVR_EXCL_LINE
    }
    m_busy = busy;
    emit busyChanged();
}

void OpenAIProviderAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
