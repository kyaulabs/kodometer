#include <kodometer/gemini_provider_adapter.hpp>

#include <QDateTime>
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

QString migrationError()
{
    return QStringLiteral(
        "Google's June 2026 shutdown ended Gemini CLI OAuth quota access for individual, AI "
        "Pro, and Ultra accounts. Use Antigravity for consumer accounts; Workspace, education, "
        "and Code Assist Standard or Enterprise accounts remain supported.");
}

QMap<QString, QString> oauthEnvironment()
{
    const QProcessEnvironment process = QProcessEnvironment::systemEnvironment();
    QMap<QString, QString> environment;
    for (const QString &key :
         {QStringLiteral("GEMINI_OAUTH_CLIENT_ID"), QStringLiteral("GEMINI_OAUTH_CLIENT_SECRET"),
          QStringLiteral("GEMINI_OAUTH2_JS_PATH"), QStringLiteral("PATH"),
          QStringLiteral("HOME")}) {
        if (process.contains(key)) { // GCOVR_EXCL_BR_LINE
            environment.insert(key, process.value(key));
        }
    }
    return environment;
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
GeminiProviderAdapter::GeminiProviderAdapter(QNetworkAccessManager *network, QObject *parent)
    : ProviderAdapter(parent),
      m_network(network == nullptr ? new QNetworkAccessManager(this) : network),
      m_credentialPath(GeminiCredentialStore::authenticationFilePath(QDir::homePath())),
      m_settingsPath(GeminiCredentialStore::settingsFilePath(QDir::homePath())),
      m_defaultCredentialPath(m_credentialPath), m_defaultSettingsPath(m_settingsPath),
      m_oauthEnvironment(oauthEnvironment())
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

QString GeminiProviderAdapter::providerId() const
{
    return QStringLiteral("gemini");
}

bool GeminiProviderAdapter::busy() const noexcept
{
    return m_busy;
}

QString GeminiProviderAdapter::error() const
{
    return m_error;
}

QVariantMap GeminiProviderAdapter::provider() const
{
    return m_provider;
}

void GeminiProviderAdapter::setCredentialPath(const QString &path)
{
    m_credentialPath = path;
    m_defaultCredentialPath = path;
}

void GeminiProviderAdapter::setSettingsPath(const QString &path)
{
    m_settingsPath = path;
    m_defaultSettingsPath = path;
}

void GeminiProviderAdapter::setProfileDirectory(const QString &directory)
{
    if (directory.isEmpty()) {
        m_credentialPath = m_defaultCredentialPath;
        m_settingsPath = m_defaultSettingsPath;
        return;
    }
    const QDir root(directory);
    m_credentialPath = root.filePath(QStringLiteral("oauth_creds.json"));
    m_settingsPath = root.filePath(QStringLiteral("settings.json"));
}

void GeminiProviderAdapter::setTokenEndpoint(const QUrl &endpoint)
{
    m_tokenEndpoint = endpoint;
}

void GeminiProviderAdapter::setCodeAssistEndpoint(const QUrl &endpoint)
{
    m_codeAssistEndpoint = endpoint;
}

void GeminiProviderAdapter::setProjectsEndpoint(const QUrl &endpoint)
{
    m_projectsEndpoint = endpoint;
}

void GeminiProviderAdapter::setQuotaEndpoint(const QUrl &endpoint)
{
    m_quotaEndpoint = endpoint;
}

void GeminiProviderAdapter::setOAuthEnvironment(const QMap<QString, QString> &environment)
{
    m_oauthEnvironment = environment;
}

void GeminiProviderAdapter::setTimeoutMilliseconds(int milliseconds)
{
    m_timeoutMilliseconds = std::max(milliseconds, 1);
}

void GeminiProviderAdapter::refresh()
{
    if (m_busy) {
        return;
    }

    // Pin both files before callbacks can change the next cycle's profile.
    m_requestCredentialPath = m_credentialPath;
    m_requestSettingsPath = m_settingsPath;
    setBusy(true);
    setError({});
    m_refreshedDuringRequest = false;
    m_codeAssistStatus = {};
    m_projectId.clear();

    const GeminiAuthType authentication =
        GeminiCredentialStore::selectedAuthentication(m_requestSettingsPath);
    if (authentication == GeminiAuthType::ApiKey) {
        completeFailure(
            QStringLiteral("Gemini API key authentication does not expose OAuth quota"));
        return;
    }
    if (authentication == GeminiAuthType::VertexAi) {
        completeFailure(
            QStringLiteral("Gemini Vertex AI authentication does not expose OAuth quota"));
        return;
    }

    QString credentialError;
    const auto credentials = GeminiCredentialStore::load(m_requestCredentialPath, &credentialError);
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
        requestCodeAssist();
    }
}

void GeminiProviderAdapter::requestToken()
{
    if (m_credentials.refreshToken.isEmpty()) {
        completeFailure(QStringLiteral("Gemini OAuth refresh token is missing"));
        return;
    }
    m_refreshedDuringRequest = true;
    const GeminiOAuthClient client = GeminiOAuthConfig::resolve(m_oauthEnvironment);
    // GCOVR_EXCL_BR_START -- tested QString accessors contain Qt branches
    if (client.clientId.isEmpty() || client.clientSecret.isEmpty()) {
        completeFailure(QStringLiteral(
            "Gemini OAuth client configuration could not be resolved; update Gemini CLI or set "
            "GEMINI_OAUTH_CLIENT_ID and GEMINI_OAUTH_CLIENT_SECRET"));
        return;
    }
    // GCOVR_EXCL_BR_STOP
    QNetworkRequest request = requestFor(m_tokenEndpoint);
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), client.clientId);
    form.addQueryItem(QStringLiteral("client_secret"), client.clientSecret);
    form.addQueryItem(QStringLiteral("refresh_token"), m_credentials.refreshToken);
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    watchReply(m_network->post(request, form.query(QUrl::FullyEncoded).toUtf8()),
               RequestKind::Token);
}

void GeminiProviderAdapter::requestCodeAssist()
{
    QNetworkRequest request = requestFor(m_codeAssistEndpoint);
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_credentials.accessToken.toUtf8());
    request.setRawHeader("Content-Type", "application/json");
    // GCOVR_EXCL_BR_START -- Qt JSON allocation branches
    const QJsonObject metadata{{QStringLiteral("ideType"), QStringLiteral("GEMINI_CLI")},
                               {QStringLiteral("pluginType"), QStringLiteral("GEMINI")}};
    const QByteArray body = QJsonDocument(QJsonObject{{QStringLiteral("metadata"), metadata}})
                                .toJson(QJsonDocument::Compact);
    // GCOVR_EXCL_BR_STOP
    watchReply(m_network->post(request, body), RequestKind::CodeAssist);
}

void GeminiProviderAdapter::requestProjects()
{
    QNetworkRequest request = requestFor(m_projectsEndpoint);
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_credentials.accessToken.toUtf8());
    watchReply(m_network->get(request), RequestKind::Projects);
}

void GeminiProviderAdapter::requestQuota()
{
    QNetworkRequest request = requestFor(m_quotaEndpoint);
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_credentials.accessToken.toUtf8());
    request.setRawHeader("Content-Type", "application/json");
    QJsonObject payload;
    if (!m_projectId.isEmpty()) {
        payload.insert(QStringLiteral("project"), m_projectId);
    }
    watchReply(m_network->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact)),
               RequestKind::Quota);
}

void GeminiProviderAdapter::watchReply(QNetworkReply *reply, RequestKind kind)
{
    m_reply = reply;
    m_requestKind = kind;
    m_responseData.clear();
    m_timedOut = false;
    m_responseTooLarge = false;
    connect(reply, &QIODevice::readyRead, this, &GeminiProviderAdapter::readReplyData);
    connect(reply, &QNetworkReply::finished, this, &GeminiProviderAdapter::finishReply);
    m_timeout.start(m_timeoutMilliseconds);
}

void GeminiProviderAdapter::readReplyData()
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

void GeminiProviderAdapter::finishReply()
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
        if (kind == RequestKind::CodeAssist || kind == RequestKind::Projects) {
            continueAfterOptionalFailure(kind);
        }
        else {
            // GCOVR_EXCL_BR_START -- QString allocation branches
            completeFailure(kind == RequestKind::Token
                                ? QStringLiteral("Gemini token response exceeds the 1 MiB limit")
                                : QStringLiteral("Gemini quota response exceeds the 1 MiB limit"));
            // GCOVR_EXCL_BR_STOP
        }
        return;
    }
    if (m_timedOut) {
        if (kind == RequestKind::CodeAssist || kind == RequestKind::Projects) {
            continueAfterOptionalFailure(kind);
        }
        else {
            // GCOVR_EXCL_BR_START -- QString allocation branches
            completeFailure(kind == RequestKind::Token
                                ? QStringLiteral("Gemini token request timed out")
                                : QStringLiteral("Gemini quota request timed out"));
            // GCOVR_EXCL_BR_STOP
        }
        return;
    }

    switch (kind) {
    case RequestKind::Token:
        finishToken(statusCode, networkError);
        break;
    case RequestKind::CodeAssist:
        finishCodeAssist(statusCode);
        break;
    case RequestKind::Projects:
        finishProjects(statusCode);
        break;
    case RequestKind::Quota:
        finishQuota(statusCode, networkError);
        break;
    case RequestKind::None: // GCOVR_EXCL_LINE -- replies always have a request kind
        break;              // GCOVR_EXCL_LINE
    }
}

void GeminiProviderAdapter::finishToken(int statusCode, QNetworkReply::NetworkError networkError)
{
    if (GeminiUsageParser::isConsumerTierDeprecation(m_responseData)) {
        completeFailure(migrationError());
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("Gemini token request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("Gemini token request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(m_responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) { // GCOVR_EXCL_BR_LINE
        completeFailure(QStringLiteral("Gemini token endpoint returned invalid JSON"));
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
        completeFailure(QStringLiteral("Gemini token endpoint returned invalid credentials"));
        return;
    }

    m_credentials.accessToken = accessToken;
    const QString refreshToken =
        response.value(QStringLiteral("refresh_token")).toString().trimmed();
    if (!refreshToken.isEmpty()) {
        m_credentials.refreshToken = refreshToken;
    }
    const QString idToken = response.value(QStringLiteral("id_token")).toString().trimmed();
    if (!idToken.isEmpty()) {
        m_credentials.idToken = idToken;
    }
    m_credentials.expiresAt =
        QDateTime::currentDateTimeUtc().addSecs(static_cast<qint64>(expiresSeconds));

    QString saveError;
    // GCOVR_EXCL_START -- failures require concurrent credential-file mutation
    if (!GeminiCredentialStore::save(m_requestCredentialPath, m_credentials, &saveError)) {
        completeFailure(saveError);
        return;
    }
    const auto reloaded = GeminiCredentialStore::load(m_requestCredentialPath, &saveError);
    if (!reloaded) {
        completeFailure(saveError);
        return;
    }
    // GCOVR_EXCL_STOP
    m_credentials = *reloaded;
    setHistoryIdentity(m_credentials.refreshToken.toUtf8(), true);
    requestCodeAssist();
}

void GeminiProviderAdapter::finishCodeAssist(int statusCode)
{
    if ((statusCode < 200 || statusCode >= 300) &&
        GeminiUsageParser::isConsumerTierDeprecation(m_responseData)) {
        completeFailure(migrationError());
        return;
    }
    const bool canRefreshUnauthorized = statusCode == 401 && !m_refreshedDuringRequest &&
                                        !m_credentials.refreshToken.isEmpty(); // GCOVR_EXCL_BR_LINE
    if (canRefreshUnauthorized) {
        requestToken();
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        requestProjects();
        return;
    }

    const auto status =
        GeminiUsageParser::parseCodeAssist(m_responseData, m_credentials.hostedDomain);
    if (!status) {
        requestProjects();
        return;
    }
    m_codeAssistStatus = *status;
    m_projectId = status->projectId;
    if (status->consumerClientUnsupported && status->tier.isEmpty()) {
        completeFailure(migrationError());
        return;
    }
    if (m_projectId.isEmpty()) {
        requestProjects();
    }
    else {
        requestQuota();
    }
}

void GeminiProviderAdapter::finishProjects(int statusCode)
{
    if (statusCode >= 200 && statusCode < 300) {
        m_projectId = GeminiUsageParser::discoverProject(m_responseData);
    }
    requestQuota();
}

void GeminiProviderAdapter::finishQuota(int statusCode, QNetworkReply::NetworkError networkError)
{
    if (GeminiUsageParser::isConsumerTierDeprecation(m_responseData)) {
        completeFailure(migrationError());
        return;
    }
    const bool canRefreshUnauthorized = statusCode == 401 && !m_refreshedDuringRequest &&
                                        !m_credentials.refreshToken.isEmpty(); // GCOVR_EXCL_BR_LINE
    if (canRefreshUnauthorized) {
        requestToken();
        return;
    }
    const bool deprecatedForbidden = statusCode == 403 &&
                                     m_codeAssistStatus.consumerClientUnsupported &&
                                     m_codeAssistStatus.tier != QStringLiteral("standard-tier");
    if (deprecatedForbidden) {
        completeFailure(migrationError());
        return;
    }
    if (statusCode == 429) {
        completeFailure(QStringLiteral("Gemini quota request was rate limited"));
        return;
    }
    if (statusCode < 200 || statusCode >= 300) {
        if (statusCode > 0) {
            completeFailure(
                QStringLiteral("Gemini quota request failed with HTTP %1").arg(statusCode));
        }
        else {
            completeFailure(QStringLiteral("Gemini quota request failed: network error %1")
                                .arg(static_cast<int>(networkError)));
        }
        return;
    }

    QString parseError;
    const auto provider =
        GeminiUsageParser::parseQuota(m_responseData, m_credentials, m_codeAssistStatus,
                                      QDateTime::currentDateTimeUtc(), &parseError);
    if (!provider) {
        completeFailure(parseError);
        return;
    }
    completeSuccess(*provider);
}

void GeminiProviderAdapter::continueAfterOptionalFailure(RequestKind kind)
{
    if (kind == RequestKind::CodeAssist) {
        requestProjects();
    }
    else {
        requestQuota();
    }
}

void GeminiProviderAdapter::completeSuccess(const QVariantMap &provider)
{
    m_provider = provider;
    emit providerChanged();
    emit refreshSucceeded(m_provider);
    setBusy(false);
    emit refreshFinished(true);
}

void GeminiProviderAdapter::completeFailure(const QString &message)
{
    setError(message);
    emit refreshFailed(message);
    setBusy(false);
    emit refreshFinished(false);
}

void GeminiProviderAdapter::setBusy(bool busy)
{
    if (m_busy == busy) { // GCOVR_EXCL_LINE -- callers only transition state
        return;           // GCOVR_EXCL_LINE
    }
    m_busy = busy;
    emit busyChanged();
}

void GeminiProviderAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
