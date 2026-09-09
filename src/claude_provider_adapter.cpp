#include <kodometer/claude_provider_adapter.hpp>

#include <kodometer/claude_usage_parser.hpp>

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>

namespace Kodometer {
namespace {

constexpr auto ClientId = "9d1c250a-e61b-44d9-88ed-5944d1962f5e";

QString defaultCredentialPath()
{
    const QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
    QMap<QString, QString> environment;
    for (const QString &key :
         {QStringLiteral("CLAUDE_CONFIG_DIR"), QStringLiteral("CLAUDE_SECURESTORAGE_CONFIG_DIR")}) {
        if (processEnvironment.contains(key)) { // GCOVR_EXCL_BR_LINE
            environment.insert(key, processEnvironment.value(key));
        }
    }
    return ClaudeCredentialStore::authenticationFilePath(environment, QDir::homePath(),
                                                         QDir::currentPath());
}

QNetworkRequest requestFor(const QUrl &endpoint)
{
    QNetworkRequest request(endpoint);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    return request;
}

} // namespace

// GCOVR_EXCL_BR_START -- Qt ownership and allocation branches
ClaudeProviderAdapter::ClaudeProviderAdapter(QNetworkAccessManager *network, QObject *parent)
    : ProviderAdapter(parent),
      m_network(network == nullptr ? new QNetworkAccessManager(this) : network),
      m_credentialPath(defaultCredentialPath()), m_defaultCredentialPath(m_credentialPath)
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

QString ClaudeProviderAdapter::providerId() const
{
    return QStringLiteral("claude");
}

bool ClaudeProviderAdapter::busy() const noexcept
{
    return m_busy;
}

QString ClaudeProviderAdapter::error() const
{
    return m_error;
}

QVariantMap ClaudeProviderAdapter::provider() const
{
    return m_provider;
}

void ClaudeProviderAdapter::setCredentialPath(const QString &path)
{
    m_credentialPath = path;
    m_defaultCredentialPath = path;
}

void ClaudeProviderAdapter::setProfileDirectory(const QString &directory)
{
    if (directory.isEmpty()) {
        m_credentialPath = m_defaultCredentialPath;
        return;
    }
    m_credentialPath = QDir(directory).filePath(QStringLiteral(".credentials.json"));
}

void ClaudeProviderAdapter::setUsageEndpoint(const QUrl &endpoint)
{
    m_usageEndpoint = endpoint;
}

void ClaudeProviderAdapter::setTokenEndpoint(const QUrl &endpoint)
{
    m_tokenEndpoint = endpoint;
}

void ClaudeProviderAdapter::setTimeoutMilliseconds(int milliseconds)
{
    m_timeoutMilliseconds = std::max(milliseconds, 1);
}

void ClaudeProviderAdapter::refresh()
{
    if (m_busy) {
        return;
    }

    setBusy(true);
    setError({});
    m_refreshedDuringRequest = false;
    QString credentialError;
    m_requestCredentialPath = m_credentialPath;
    const auto credentials = ClaudeCredentialStore::load(m_requestCredentialPath, &credentialError);
    if (!credentials) {
        completeFailure(credentialError);
        return;
    }
    m_credentials = *credentials;
    setHistoryIdentity((m_credentials.refreshToken.isEmpty() ? m_credentials.accessToken
                                                             : m_credentials.refreshToken)
                           .toUtf8());

    if (m_credentials.needsRefresh()) {
        requestToken();
    }
    else {
        requestUsage();
    }
}

void ClaudeProviderAdapter::requestUsage()
{
    QNetworkRequest request = requestFor(m_usageEndpoint);
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_credentials.accessToken.toUtf8());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("anthropic-beta", "oauth-2025-04-20");
    request.setRawHeader("User-Agent", "claude-code/2.1.0");
    watchReply(m_network->get(request), RequestKind::Usage);
}

void ClaudeProviderAdapter::requestToken()
{
    if (m_credentials.refreshToken.isEmpty()) {
        completeFailure(QStringLiteral("Claude OAuth refresh token is missing"));
        return;
    }
    m_refreshedDuringRequest = true;
    QNetworkRequest request = requestFor(m_tokenEndpoint);
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("refresh_token"), m_credentials.refreshToken);
    form.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(ClientId));
    watchReply(m_network->post(request, form.query(QUrl::FullyEncoded).toUtf8()),
               RequestKind::Token);
}

void ClaudeProviderAdapter::watchReply(QNetworkReply *reply, RequestKind kind)
{
    m_reply = reply;
    m_requestKind = kind;
    m_responseData.clear();
    m_timedOut = false;
    m_responseTooLarge = false;
    connect(reply, &QIODevice::readyRead, this, &ClaudeProviderAdapter::readReplyData);
    connect(reply, &QNetworkReply::finished, this, &ClaudeProviderAdapter::finishReply);
    m_timeout.start(m_timeoutMilliseconds);
}

void ClaudeProviderAdapter::readReplyData()
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

void ClaudeProviderAdapter::finishReply()
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
                            ? QStringLiteral("Claude usage response exceeds the 1 MiB limit")
                            : QStringLiteral("Claude token response exceeds the 1 MiB limit"));
        // GCOVR_EXCL_BR_STOP
        return;
    }
    if (m_timedOut) {
        // GCOVR_EXCL_BR_START -- QString allocation branches
        completeFailure(kind == RequestKind::Usage
                            ? QStringLiteral("Claude usage request timed out")
                            : QStringLiteral("Claude token request timed out"));
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

void ClaudeProviderAdapter::finishUsage(int statusCode, QNetworkReply::NetworkError networkError)
{
    const bool canRefreshUnauthorized = statusCode == 401 && !m_refreshedDuringRequest &&
                                        !m_credentials.refreshToken.isEmpty(); // GCOVR_EXCL_BR_LINE
    if (canRefreshUnauthorized) {
        requestToken();
        return;
    }
    if (statusCode == 429) {
        completeFailure(QStringLiteral("Claude usage request was rate limited"));
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("Claude usage request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("Claude usage request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QString parseError;
    const auto provider = ClaudeUsageParser::parse(m_responseData, m_credentials,
                                                   QDateTime::currentDateTimeUtc(), &parseError);
    if (!provider) {
        completeFailure(parseError);
        return;
    }
    completeSuccess(*provider);
}

void ClaudeProviderAdapter::finishToken(int statusCode, QNetworkReply::NetworkError networkError)
{
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("Claude token request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("Claude token request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(m_responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) { // GCOVR_EXCL_BR_LINE
        completeFailure(QStringLiteral("Claude token endpoint returned invalid JSON"));
        return;
    }
    const QJsonObject response = document.object();
    const QString accessToken = response.value(QStringLiteral("access_token")).toString().trimmed();
    const QJsonValue expiresValue = response.value(QStringLiteral("expires_in"));
    const double expiresSeconds = expiresValue.toDouble(-1.0);
    const bool validExpiration = expiresValue.isDouble() && std::isfinite(expiresSeconds) &&
                                 std::floor(expiresSeconds) == expiresSeconds &&
                                 expiresSeconds > 0.0 &&
                                 expiresSeconds <= 31'536'000.0; // GCOVR_EXCL_BR_LINE
    if (accessToken.isEmpty() || !validExpiration) {
        completeFailure(QStringLiteral("Claude token endpoint returned invalid credentials"));
        return;
    }

    m_credentials.accessToken = accessToken;
    const QString refreshToken =
        response.value(QStringLiteral("refresh_token")).toString().trimmed();
    if (!refreshToken.isEmpty()) {
        m_credentials.refreshToken = refreshToken;
    }
    m_credentials.expiresAt =
        QDateTime::currentDateTimeUtc().addSecs(static_cast<qint64>(expiresSeconds));

    QString saveError;
    // GCOVR_EXCL_START -- failures require concurrent credential-file mutation
    if (!ClaudeCredentialStore::save(m_requestCredentialPath, m_credentials, &saveError)) {
        completeFailure(saveError);
        return;
    }
    const auto reloaded = ClaudeCredentialStore::load(m_requestCredentialPath, &saveError);
    if (!reloaded) {
        completeFailure(saveError);
        return;
    }
    // GCOVR_EXCL_STOP
    m_credentials = *reloaded;
    setHistoryIdentity(m_credentials.refreshToken.toUtf8(), true);
    requestUsage();
}

void ClaudeProviderAdapter::completeSuccess(const QVariantMap &provider)
{
    m_provider = provider;
    emit providerChanged();
    emit refreshSucceeded(m_provider);
    setBusy(false);
    emit refreshFinished(true);
}

void ClaudeProviderAdapter::completeFailure(const QString &message)
{
    setError(message);
    emit refreshFailed(message);
    setBusy(false);
    emit refreshFinished(false);
}

void ClaudeProviderAdapter::setBusy(bool busy)
{
    if (m_busy == busy) { // GCOVR_EXCL_LINE -- callers only transition state
        return;           // GCOVR_EXCL_LINE
    }
    m_busy = busy;
    emit busyChanged();
}

void ClaudeProviderAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
