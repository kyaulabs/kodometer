#include <kodometer/gemini_provider_adapter.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

using Kodometer::GeminiProviderAdapter;

namespace {

struct HttpRequest
{
    QByteArray method;
    QByteArray path;
    QMap<QByteArray, QByteArray> headers;
    QByteArray body;
};

struct HttpResponse
{
    HttpResponse(int responseStatus = 200, QByteArray responseBody = "{}",
                 QMap<QByteArray, QByteArray> responseHeaders = {}, bool shouldSend = true)
        : status(responseStatus), body(std::move(responseBody)),
          headers(std::move(responseHeaders)), send(shouldSend)
    {}

    int status;
    QByteArray body;
    QMap<QByteArray, QByteArray> headers;
    bool send;
};

class HttpServer final : public QObject
{
  public:
    HttpServer()
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    m_buffers[socket].append(socket->readAll());
                    process(socket);
                });
            }
        });
        Q_ASSERT(m_server.listen(QHostAddress::LocalHost));
    }

    void enqueue(HttpResponse response)
    {
        m_responses.enqueue(std::move(response));
    }

    [[nodiscard]] QUrl url(const QString &path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path));
    }

    QList<HttpRequest> requests;

  private:
    void process(QTcpSocket *socket)
    {
        const QByteArray data = m_buffers.value(socket);
        const qsizetype headerEnd = data.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }
        const QList<QByteArray> lines = data.left(headerEnd).split('\n');
        const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
        if (requestLine.size() < 2) {
            return;
        }

        HttpRequest request;
        request.method = requestLine.at(0);
        request.path = requestLine.at(1);
        for (qsizetype index = 1; index < lines.size(); ++index) {
            const QByteArray line = lines.at(index).trimmed();
            const qsizetype separator = line.indexOf(':');
            if (separator > 0) {
                request.headers.insert(line.left(separator).toLower(),
                                       line.mid(separator + 1).trimmed());
            }
        }
        bool lengthValid = false;
        const qsizetype contentLength =
            request.headers.value("content-length").toLongLong(&lengthValid);
        const qsizetype bodyStart = headerEnd + 4;
        if (lengthValid && data.size() - bodyStart < contentLength) {
            return;
        }
        request.body = data.mid(bodyStart, lengthValid ? contentLength : 0);
        requests.append(request);
        m_buffers.remove(socket);

        if (m_responses.isEmpty()) {
            return;
        }
        const HttpResponse response = m_responses.dequeue();
        if (!response.send) {
            return;
        }
        const QByteArray reason = response.status >= 400 ? "Error" : "OK";
        QByteArray output =
            "HTTP/1.1 " + QByteArray::number(response.status) + ' ' + reason + "\r\n";
        output += "Content-Type: application/json\r\n";
        output += "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n";
        for (auto iterator = response.headers.cbegin(); iterator != response.headers.cend();
             ++iterator) {
            output += iterator.key() + ": " + iterator.value() + "\r\n";
        }
        output += "Connection: close\r\n\r\n" + response.body;
        socket->write(output);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QQueue<HttpResponse> m_responses;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

QString identityToken()
{
    const QByteArray payload =
        QJsonDocument(QJsonObject{{QStringLiteral("email"), QStringLiteral("person@example.com")}})
            .toJson(QJsonDocument::Compact)
            .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QStringLiteral("header.%1.signature").arg(QString::fromLatin1(payload));
}

bool writeCredentials(const QString &path, qint64 expiresAt,
                      const QString &refreshToken = QStringLiteral("refresh-token"))
{
    const QJsonObject root{{QStringLiteral("access_token"), QStringLiteral("access-token")},
                           {QStringLiteral("refresh_token"), refreshToken},
                           {QStringLiteral("id_token"), identityToken()},
                           {QStringLiteral("expiry_date"), expiresAt},
                           {QStringLiteral("preserved"), 7}};
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson()) <= 0) {
        return false;
    }
    file.close();
    return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

bool writeSettings(const QString &path, const QString &selectedType)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QJsonObject root{
        {QStringLiteral("security"),
         QJsonObject{{QStringLiteral("auth"),
                      QJsonObject{{QStringLiteral("selectedType"), selectedType}}}}}};
    return file.write(QJsonDocument(root).toJson()) > 0;
}

QByteArray quotaPayload()
{
    return R"({"buckets":[{"modelId":"gemini-2.5-pro","remainingFraction":0.75,"resetTime":"2026-09-06T00:00:00Z"}]})";
}

void configure(GeminiProviderAdapter &adapter, const QString &credentials, const QString &settings,
               const HttpServer &server)
{
    adapter.setCredentialPath(credentials);
    adapter.setSettingsPath(settings);
    adapter.setTokenEndpoint(server.url(QStringLiteral("/token")));
    adapter.setCodeAssistEndpoint(server.url(QStringLiteral("/code-assist")));
    adapter.setProjectsEndpoint(server.url(QStringLiteral("/projects")));
    adapter.setQuotaEndpoint(server.url(QStringLiteral("/quota")));
    adapter.setOAuthEnvironment(
        {{QStringLiteral("GEMINI_OAUTH_CLIENT_ID"), QStringLiteral("test-client")},
         {QStringLiteral("GEMINI_OAUTH_CLIENT_SECRET"), QStringLiteral("test-secret")}});
}

} // namespace

class GeminiProviderAdapterTest final : public QObject
{
    Q_OBJECT

  private slots:
    void fetchesCodeAssistAndQuotaWithExistingToken();
    void refreshesExpiringTokenAndPersistsRotation();
    void discoversProjectWhenCodeAssistHasNone();
    void retriesUnauthorizedQuotaOnce();
    void retriesUnauthorizedCodeAssistOnce();
    void reportsAuthenticationAndMigrationFailures();
    void reportsTokenAndQuotaFailures();
    void handlesOptionalProbeFailuresAndLimitsResponses();
    void rejectsConcurrentRefreshAndTimesOut();
};

void GeminiProviderAdapterTest::fetchesCodeAssistAndQuotaWithExistingToken()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString credentials = directory.filePath(QStringLiteral("oauth_creds.json"));
    const QString settings = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeCredentials(credentials, 2'000'000'000'000));
    QVERIFY(writeSettings(settings, QStringLiteral("oauth-personal")));
    HttpServer server;
    server.enqueue(
        {200, R"({"cloudaicompanionProject":"project-1","currentTier":{"id":"standard-tier"}})"});
    server.enqueue({200, quotaPayload()});
    QNetworkAccessManager network;
    GeminiProviderAdapter adapter(&network);
    configure(adapter, credentials, settings, server);
    QSignalSpy finished(&adapter, &GeminiProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 2);
    QCOMPARE(server.requests.at(0).method, QByteArray("POST"));
    QCOMPARE(server.requests.at(0).path, QByteArray("/code-assist"));
    QCOMPARE(server.requests.at(0).headers.value("authorization"),
             QByteArray("Bearer access-token"));
    QVERIFY(server.requests.at(0).body.contains("GEMINI_CLI"));
    QCOMPARE(server.requests.at(1).path, QByteArray("/quota"));
    QCOMPARE(QJsonDocument::fromJson(server.requests.at(1).body)
                 .object()
                 .value(QStringLiteral("project"))
                 .toString(),
             QStringLiteral("project-1"));
    QCOMPARE(adapter.provider().value(QStringLiteral("id")).toString(), QStringLiteral("gemini"));
    QCOMPARE(adapter.provider()
                 .value(QStringLiteral("identity"))
                 .toMap()
                 .value(QStringLiteral("plan"))
                 .toString(),
             QStringLiteral("Paid"));
    QVERIFY(!adapter.busy());
    QVERIFY(adapter.error().isEmpty());
}

void GeminiProviderAdapterTest::refreshesExpiringTokenAndPersistsRotation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString credentials = directory.filePath(QStringLiteral("oauth_creds.json"));
    const QString settings = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeCredentials(credentials, 1));
    HttpServer server;
    server.enqueue(
        {200,
         R"({"access_token":"new-access","refresh_token":"new-refresh","id_token":"new-id","expires_in":3600,"token_type":"Bearer"})"});
    server.enqueue(
        {200,
         R"({"cloudaicompanionProject":{"id":"project-2"},"currentTier":{"id":"free-tier"}})"});
    server.enqueue({200, quotaPayload()});
    QNetworkAccessManager network;
    GeminiProviderAdapter adapter(&network);
    configure(adapter, credentials, settings, server);
    QSignalSpy finished(&adapter, &GeminiProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 3);
    QCOMPARE(server.requests.at(0).path, QByteArray("/token"));
    QVERIFY(server.requests.at(0).body.contains("grant_type=refresh_token"));
    QVERIFY(server.requests.at(0).body.contains("refresh_token=refresh-token"));
    QVERIFY(server.requests.at(0).body.contains("client_id=test-client"));
    QVERIFY(server.requests.at(0).body.contains("client_secret=test-secret"));
    QCOMPARE(server.requests.at(1).headers.value("authorization"), QByteArray("Bearer new-access"));

    QFile file(credentials);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    QCOMPARE(root.value(QStringLiteral("access_token")).toString(), QStringLiteral("new-access"));
    QCOMPARE(root.value(QStringLiteral("refresh_token")).toString(), QStringLiteral("new-refresh"));
    QCOMPARE(root.value(QStringLiteral("id_token")).toString(), QStringLiteral("new-id"));
    QCOMPARE(root.value(QStringLiteral("preserved")).toInt(), 7);
    QVERIFY(root.value(QStringLiteral("expiry_date")).toVariant().toLongLong() >
            QDateTime::currentMSecsSinceEpoch());
}

void GeminiProviderAdapterTest::discoversProjectWhenCodeAssistHasNone()
{
    QTemporaryDir directory;
    const QString credentials = directory.filePath(QStringLiteral("oauth_creds.json"));
    const QString settings = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeCredentials(credentials, 2'000'000'000'000));
    HttpServer server;
    server.enqueue({200, R"({"currentTier":{"id":"legacy-tier"}})"});
    server.enqueue({200, R"({"projects":[{"projectId":"gen-lang-client-discovered"}]})"});
    server.enqueue({200, quotaPayload()});
    QNetworkAccessManager network;
    GeminiProviderAdapter adapter(&network);
    configure(adapter, credentials, settings, server);
    QSignalSpy finished(&adapter, &GeminiProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(server.requests.size(), 3);
    QCOMPARE(server.requests.at(1).method, QByteArray("GET"));
    QCOMPARE(server.requests.at(1).path, QByteArray("/projects"));
    QCOMPARE(QJsonDocument::fromJson(server.requests.at(2).body)
                 .object()
                 .value(QStringLiteral("project"))
                 .toString(),
             QStringLiteral("gen-lang-client-discovered"));
}

void GeminiProviderAdapterTest::retriesUnauthorizedQuotaOnce()
{
    QTemporaryDir directory;
    const QString credentials = directory.filePath(QStringLiteral("oauth_creds.json"));
    const QString settings = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeCredentials(credentials, 2'000'000'000'000));
    HttpServer server;
    const QByteArray status =
        R"({"cloudaicompanionProject":"project","currentTier":{"id":"free-tier"}})";
    server.enqueue({200, status});
    server.enqueue({401, "{}"});
    server.enqueue({200, R"({"access_token":"retried","expires_in":3600})"});
    server.enqueue({200, status});
    server.enqueue({200, quotaPayload()});
    QNetworkAccessManager network;
    GeminiProviderAdapter adapter(&network);
    configure(adapter, credentials, settings, server);
    QSignalSpy finished(&adapter, &GeminiProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 5);
    QCOMPARE(server.requests.at(4).headers.value("authorization"), QByteArray("Bearer retried"));
}

void GeminiProviderAdapterTest::retriesUnauthorizedCodeAssistOnce()
{
    QTemporaryDir directory;
    const QString credentials = directory.filePath(QStringLiteral("oauth_creds.json"));
    const QString settings = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeCredentials(credentials, 2'000'000'000'000));
    HttpServer server;
    server.enqueue({401, "{}"});
    server.enqueue({200, R"({"access_token":"retried-code-assist","expires_in":3600})"});
    server.enqueue({200, R"({"cloudaicompanionProject":"project"})"});
    server.enqueue({200, quotaPayload()});
    QNetworkAccessManager network;
    GeminiProviderAdapter adapter(&network);
    configure(adapter, credentials, settings, server);
    QSignalSpy finished(&adapter, &GeminiProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 4);
    QCOMPARE(server.requests.at(2).headers.value("authorization"),
             QByteArray("Bearer retried-code-assist"));
}

void GeminiProviderAdapterTest::reportsAuthenticationAndMigrationFailures()
{
    QTemporaryDir directory;
    const QString credentials = directory.filePath(QStringLiteral("oauth_creds.json"));
    const QString settings = directory.filePath(QStringLiteral("settings.json"));
    HttpServer server;
    QNetworkAccessManager network;
    GeminiProviderAdapter adapter(&network);
    configure(adapter, credentials, settings, server);
    QSignalSpy finished(&adapter, &GeminiProviderAdapter::refreshFinished);

    adapter.refresh();
    QCOMPARE(finished.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini credentials were not found"));

    QVERIFY(writeCredentials(credentials, 2'000'000'000'000));
    QVERIFY(writeSettings(settings, QStringLiteral("api-key")));
    adapter.refresh();
    QCOMPARE(finished.count(), 2);
    QCOMPARE(adapter.error(),
             QStringLiteral("Gemini API key authentication does not expose OAuth quota"));

    QVERIFY(writeSettings(settings, QStringLiteral("vertex-ai")));
    adapter.refresh();
    QCOMPARE(finished.count(), 3);
    QCOMPARE(adapter.error(),
             QStringLiteral("Gemini Vertex AI authentication does not expose OAuth quota"));

    QVERIFY(writeSettings(settings, QStringLiteral("oauth-personal")));
    server.enqueue({200, R"({"ineligibleTiers":[{"reasonCode":"UNSUPPORTED_CLIENT"}]})"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 4);
    QVERIFY(adapter.error().contains(QStringLiteral("June 2026")));
    QVERIFY(adapter.error().contains(QStringLiteral("Antigravity")));

    server.enqueue({403, R"({"error":"unsupported_client"})"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 5);
    QVERIFY(adapter.error().contains(QStringLiteral("June 2026")));

    QVERIFY(writeCredentials(credentials, 1));
    server.enqueue({400, R"({"error":"unsupported_client"})"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 6);
    QVERIFY(adapter.error().contains(QStringLiteral("June 2026")));

    QVERIFY(writeCredentials(credentials, 2'000'000'000'000));
    server.enqueue(
        {200,
         R"({"cloudaicompanionProject":"project","currentTier":{"id":"free-tier"},"ineligibleTiers":[{"reasonCode":"UNSUPPORTED_CLIENT"}]})"});
    server.enqueue({403, "{}"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 7);
    QVERIFY(adapter.error().contains(QStringLiteral("June 2026")));

    server.enqueue({200, R"({"cloudaicompanionProject":"project"})"});
    server.enqueue({403, "Gemini Code Assist is no longer supported"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 8);
    QVERIFY(adapter.error().contains(QStringLiteral("June 2026")));

    QVERIFY(writeCredentials(credentials, 1));
    adapter.setOAuthEnvironment(
        {{QStringLiteral("PATH"), directory.path()}, {QStringLiteral("HOME"), directory.path()}});
    adapter.refresh();
    QCOMPARE(finished.count(), 9);
    QVERIFY(adapter.error().startsWith(
        QStringLiteral("Gemini OAuth client configuration could not be resolved")));

    adapter.setOAuthEnvironment(
        {{QStringLiteral("GEMINI_OAUTH_CLIENT_ID"), QStringLiteral("test-client")},
         {QStringLiteral("GEMINI_OAUTH_CLIENT_SECRET"), QStringLiteral("test-secret")}});
    QVERIFY(writeCredentials(credentials, 1, {}));
    adapter.refresh();
    QCOMPARE(finished.count(), 10);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini OAuth refresh token is missing"));
}

void GeminiProviderAdapterTest::reportsTokenAndQuotaFailures()
{
    QTemporaryDir directory;
    const QString credentials = directory.filePath(QStringLiteral("oauth_creds.json"));
    const QString settings = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeCredentials(credentials, 1));
    HttpServer server;
    QNetworkAccessManager network;
    GeminiProviderAdapter adapter(&network);
    configure(adapter, credentials, settings, server);
    QSignalSpy finished(&adapter, &GeminiProviderAdapter::refreshFinished);

    server.enqueue({400, R"({"error":"invalid_grant"})"});
    adapter.refresh();
    QVERIFY(finished.wait());
    QCOMPARE(adapter.error(), QStringLiteral("Gemini token request failed with HTTP 400"));

    server.enqueue({200, "{"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini token endpoint returned invalid JSON"));

    server.enqueue({200, R"({"access_token":"","expires_in":3600})"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini token endpoint returned invalid credentials"));

    server.enqueue({200, R"({"access_token":"token","expires_in":"later"})"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 4);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini token endpoint returned invalid credentials"));

    adapter.setTokenEndpoint(QUrl(QStringLiteral("http://127.0.0.1:1/token")));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 5);
    QVERIFY(
        adapter.error().startsWith(QStringLiteral("Gemini token request failed: network error")));

    adapter.setTokenEndpoint(server.url(QStringLiteral("/token")));
    adapter.setTimeoutMilliseconds(20);
    server.enqueue({200, "{}", {}, false});
    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 6, 1000);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini token request timed out"));

    adapter.setTimeoutMilliseconds(1000);
    server.enqueue({200, QByteArray(GeminiProviderAdapter::MaximumResponseSize + 1, 'x')});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 7);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini token response exceeds the 1 MiB limit"));

    QVERIFY(writeCredentials(credentials, 2'000'000'000'000));
    const QByteArray status =
        R"({"cloudaicompanionProject":"project","currentTier":{"id":"standard-tier"}})";
    server.enqueue({200, status});
    server.enqueue({429, "{}"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 8);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini quota request was rate limited"));

    server.enqueue({200, status});
    server.enqueue({500, "{}"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 9);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini quota request failed with HTTP 500"));

    server.enqueue(
        {200,
         R"({"cloudaicompanionProject":"project","currentTier":{"id":"standard-tier"},"ineligibleTiers":[{"reasonCode":"UNSUPPORTED_CLIENT"}]})"});
    server.enqueue({403, "{}"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 10);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini quota request failed with HTTP 403"));

    server.enqueue({200, status});
    server.enqueue({200, "{"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 11);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini quota API returned invalid JSON"));

    adapter.setCodeAssistEndpoint(QUrl(QStringLiteral("http://127.0.0.1:1/code")));
    adapter.setProjectsEndpoint(QUrl(QStringLiteral("http://127.0.0.1:1/projects")));
    adapter.setQuotaEndpoint(QUrl(QStringLiteral("http://127.0.0.1:1/quota")));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 12);
    QVERIFY(
        adapter.error().startsWith(QStringLiteral("Gemini quota request failed: network error")));
}

void GeminiProviderAdapterTest::handlesOptionalProbeFailuresAndLimitsResponses()
{
    QTemporaryDir directory;
    const QString credentials = directory.filePath(QStringLiteral("oauth_creds.json"));
    const QString settings = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeCredentials(credentials, 2'000'000'000'000));
    HttpServer server;
    QNetworkAccessManager network;
    GeminiProviderAdapter adapter(&network);
    configure(adapter, credentials, settings, server);
    QSignalSpy finished(&adapter, &GeminiProviderAdapter::refreshFinished);

    server.enqueue({500, "{}"});
    server.enqueue({500, "{}"});
    server.enqueue({200, quotaPayload()});
    adapter.refresh();
    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 3);
    QCOMPARE(server.requests.at(2).body, QByteArray("{}"));

    server.enqueue({200, "{"});
    server.enqueue({200, QByteArray(GeminiProviderAdapter::MaximumResponseSize + 1, 'x')});
    server.enqueue({200, quotaPayload()});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).first().toBool(), true);

    server.enqueue({200, QByteArray(GeminiProviderAdapter::MaximumResponseSize + 1, 'x')});
    server.enqueue({500, "{}"});
    server.enqueue({200, quotaPayload()});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(finished.at(2).first().toBool(), true);

    const QByteArray status = R"({"cloudaicompanionProject":"project"})";
    server.enqueue({200, status});
    server.enqueue({200, QByteArray(GeminiProviderAdapter::MaximumResponseSize + 1, 'x')});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 4);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini quota response exceeds the 1 MiB limit"));
}

void GeminiProviderAdapterTest::rejectsConcurrentRefreshAndTimesOut()
{
    QTemporaryDir directory;
    const QString credentials = directory.filePath(QStringLiteral("oauth_creds.json"));
    const QString settings = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeCredentials(credentials, 2'000'000'000'000));
    HttpServer server;
    server.enqueue({200, "{}", {}, false});
    server.enqueue({200, "{}", {}, false});
    server.enqueue({200, "{}", {}, false});
    QNetworkAccessManager network;
    GeminiProviderAdapter adapter(&network);
    configure(adapter, credentials, settings, server);
    adapter.setTimeoutMilliseconds(20);
    QSignalSpy finished(&adapter, &GeminiProviderAdapter::refreshFinished);

    adapter.refresh();
    adapter.refresh();

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);
    QCOMPARE(server.requests.size(), 3);
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), QStringLiteral("Gemini quota request timed out"));
}

QTEST_GUILESS_MAIN(GeminiProviderAdapterTest)

#include "tst_gemini_provider_adapter.moc"
