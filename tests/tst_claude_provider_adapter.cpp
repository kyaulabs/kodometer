#include <kodometer/claude_provider_adapter.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

using Kodometer::ClaudeProviderAdapter;

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

bool writeCredentials(const QString &path, qint64 expiresAt,
                      const QString &refreshToken = QStringLiteral("refresh-token"))
{
    const QJsonObject oauth{
        {QStringLiteral("accessToken"), QStringLiteral("access-token")},
        {QStringLiteral("refreshToken"), refreshToken},
        {QStringLiteral("expiresAt"), expiresAt},
        {QStringLiteral("scopes"), QJsonArray{QStringLiteral("user:profile")}},
        {QStringLiteral("rateLimitTier"), QStringLiteral("pro")},
    };
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("claudeAiOauth"), oauth}}).toJson());
    file.close();
    return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QByteArray usagePayload()
{
    return R"({"five_hour":{"utilization":25,"resets_at":"2026-09-04T10:00:00Z"}})";
}

} // namespace

class ClaudeProviderAdapterTest final : public QObject
{
    Q_OBJECT

  private slots:
    void fetchesUsageWithExistingToken();
    void refreshesExpiringTokenBeforeUsage();
    void retriesUnauthorizedUsageOnce();
    void reportsCredentialAndResponseFailures();
    void reportsTokenAndNetworkFailures();
    void rejectsConcurrentRefreshAndTimesOut();
    void limitsResponseSize();
};

void ClaudeProviderAdapterTest::fetchesUsageWithExistingToken()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral(".credentials.json"));
    QVERIFY(writeCredentials(path, 2'000'000'000'000));
    HttpServer server;
    server.enqueue({200, usagePayload()});
    QNetworkAccessManager network;
    ClaudeProviderAdapter adapter(&network);
    adapter.setCredentialPath(path);
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    QSignalSpy finished(&adapter, &ClaudeProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 1);
    QCOMPARE(server.requests.first().method, QByteArray("GET"));
    QCOMPARE(server.requests.first().path, QByteArray("/usage"));
    QCOMPARE(server.requests.first().headers.value("authorization"),
             QByteArray("Bearer access-token"));
    QCOMPARE(server.requests.first().headers.value("anthropic-beta"),
             QByteArray("oauth-2025-04-20"));
    QCOMPARE(server.requests.first().headers.value("user-agent"), QByteArray("claude-code/2.1.0"));
    QCOMPARE(adapter.provider().value(QStringLiteral("id")).toString(), QStringLiteral("claude"));
    QVERIFY(!adapter.busy());
    QVERIFY(adapter.error().isEmpty());
}

void ClaudeProviderAdapterTest::refreshesExpiringTokenBeforeUsage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral(".credentials.json"));
    QVERIFY(writeCredentials(path, 1));
    HttpServer server;
    server.enqueue(
        {200,
         R"({"access_token":"new-access","refresh_token":"new-refresh","expires_in":3600,"token_type":"Bearer"})"});
    server.enqueue({200, usagePayload()});
    QNetworkAccessManager network;
    ClaudeProviderAdapter adapter(&network);
    adapter.setCredentialPath(path);
    adapter.setTokenEndpoint(server.url(QStringLiteral("/token")));
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    QSignalSpy finished(&adapter, &ClaudeProviderAdapter::refreshFinished);
    QTemporaryDir other;
    QVERIFY(other.isValid());
    adapter.setProfileDirectory(directory.path());
    adapter.refresh();
    adapter.setProfileDirectory(other.path());

    QVERIFY(finished.wait());
    QVERIFY(!QFile::exists(other.filePath(QStringLiteral(".credentials.json"))));
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 2);
    QCOMPARE(server.requests.at(0).method, QByteArray("POST"));
    QCOMPARE(server.requests.at(0).headers.value("content-type"),
             QByteArray("application/x-www-form-urlencoded"));
    QVERIFY(server.requests.at(0).body.contains("grant_type=refresh_token"));
    QVERIFY(server.requests.at(0).body.contains("refresh_token=refresh-token"));
    QVERIFY(server.requests.at(0).body.contains("client_id=9d1c250a-e61b-44d9-88ed-5944d1962f5e"));
    QCOMPARE(server.requests.at(1).headers.value("authorization"), QByteArray("Bearer new-access"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject oauth = QJsonDocument::fromJson(file.readAll())
                                  .object()
                                  .value(QStringLiteral("claudeAiOauth"))
                                  .toObject();
    QCOMPARE(oauth.value(QStringLiteral("accessToken")).toString(), QStringLiteral("new-access"));
    QCOMPARE(oauth.value(QStringLiteral("refreshToken")).toString(), QStringLiteral("new-refresh"));
    QVERIFY(oauth.value(QStringLiteral("expiresAt")).toVariant().toLongLong() >
            QDateTime::currentMSecsSinceEpoch());
    adapter.setProfileDirectory({});
    server.enqueue({200, usagePayload()});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.last().first().toBool(), true);
    QCOMPARE(server.requests.last().headers.value("authorization"),
             QByteArray("Bearer new-access"));
    QVERIFY(
        writeCredentials(other.filePath(QStringLiteral(".credentials.json")), 2'000'000'000'000));
    adapter.setProfileDirectory(other.path());
    server.enqueue({200, usagePayload()});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(finished.last().first().toBool(), true);
    QCOMPARE(server.requests.last().headers.value("authorization"),
             QByteArray("Bearer access-token"));
}

void ClaudeProviderAdapterTest::retriesUnauthorizedUsageOnce()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral(".credentials.json"));
    QVERIFY(writeCredentials(path, 2'000'000'000'000));
    HttpServer server;
    server.enqueue({401, "{}"});
    server.enqueue({200, R"({"access_token":"retried","expires_in":3600})"});
    server.enqueue({200, usagePayload()});
    QNetworkAccessManager network;
    ClaudeProviderAdapter adapter(&network);
    adapter.setCredentialPath(path);
    adapter.setTokenEndpoint(server.url(QStringLiteral("/token")));
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    QSignalSpy finished(&adapter, &ClaudeProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 3);
    QCOMPARE(server.requests.at(2).headers.value("authorization"), QByteArray("Bearer retried"));
}

void ClaudeProviderAdapterTest::reportsCredentialAndResponseFailures()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral(".credentials.json"));
    QNetworkAccessManager network;
    ClaudeProviderAdapter adapter(&network);
    adapter.setCredentialPath(path);
    QSignalSpy finished(&adapter, &ClaudeProviderAdapter::refreshFinished);

    adapter.refresh();
    QCOMPARE(finished.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("Claude credentials were not found"));

    QVERIFY(writeCredentials(path, 2'000'000'000'000));
    HttpServer server;
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    server.enqueue({429, "{}", {{QByteArray("Retry-After"), QByteArray("60")}}});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(adapter.error(), QStringLiteral("Claude usage request was rate limited"));

    server.enqueue({200, "{"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(adapter.error(), QStringLiteral("Claude usage API returned invalid JSON"));

    QVERIFY(writeCredentials(path, 1, {}));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 4);
    QCOMPARE(adapter.error(), QStringLiteral("Claude OAuth refresh token is missing"));
}

void ClaudeProviderAdapterTest::reportsTokenAndNetworkFailures()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral(".credentials.json"));
    QVERIFY(writeCredentials(path, 1));
    HttpServer server;
    QNetworkAccessManager network;
    ClaudeProviderAdapter adapter(&network);
    adapter.setCredentialPath(path);
    adapter.setTokenEndpoint(server.url(QStringLiteral("/token")));
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    QSignalSpy finished(&adapter, &ClaudeProviderAdapter::refreshFinished);

    server.enqueue({400, R"({"error":"invalid_grant"})"});
    adapter.refresh();
    QVERIFY(finished.wait());
    QCOMPARE(adapter.error(), QStringLiteral("Claude token request failed with HTTP 400"));

    server.enqueue({200, "{"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(adapter.error(), QStringLiteral("Claude token endpoint returned invalid JSON"));

    server.enqueue({200, R"({"access_token":"","expires_in":3600})"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(adapter.error(), QStringLiteral("Claude token endpoint returned invalid credentials"));

    server.enqueue({200, R"({"access_token":"token","expires_in":"later"})"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 4);
    QCOMPARE(adapter.error(), QStringLiteral("Claude token endpoint returned invalid credentials"));

    adapter.setTimeoutMilliseconds(20);
    server.enqueue({200, "{}", {}, false});
    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 5, 1000);
    QCOMPARE(adapter.error(), QStringLiteral("Claude token request timed out"));

    adapter.setTimeoutMilliseconds(1000);
    server.enqueue({200, QByteArray(ClaudeProviderAdapter::MaximumResponseSize + 1, 'x')});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 6);
    QCOMPARE(adapter.error(), QStringLiteral("Claude token response exceeds the 1 MiB limit"));

    adapter.setTokenEndpoint(QUrl(QStringLiteral("http://127.0.0.1:1/token")));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 7);
    QVERIFY(
        adapter.error().startsWith(QStringLiteral("Claude token request failed: network error")));

    QVERIFY(writeCredentials(path, 2'000'000'000'000));
    adapter.setUsageEndpoint(QUrl(QStringLiteral("http://127.0.0.1:1/usage")));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 8);
    QVERIFY(
        adapter.error().startsWith(QStringLiteral("Claude usage request failed: network error")));

    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    server.enqueue({500, "{}"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 9);
    QCOMPARE(adapter.error(), QStringLiteral("Claude usage request failed with HTTP 500"));

    server.enqueue({200, usagePayload()});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 10);
    QVERIFY(adapter.error().isEmpty());
}

void ClaudeProviderAdapterTest::rejectsConcurrentRefreshAndTimesOut()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral(".credentials.json"));
    QVERIFY(writeCredentials(path, 2'000'000'000'000));
    HttpServer server;
    server.enqueue({200, "{}", {}, false});
    QNetworkAccessManager network;
    ClaudeProviderAdapter adapter(&network);
    adapter.setCredentialPath(path);
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    adapter.setTimeoutMilliseconds(20);
    QSignalSpy finished(&adapter, &ClaudeProviderAdapter::refreshFinished);

    adapter.refresh();
    adapter.refresh();

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);
    QCOMPARE(server.requests.size(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), QStringLiteral("Claude usage request timed out"));
}

void ClaudeProviderAdapterTest::limitsResponseSize()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral(".credentials.json"));
    QVERIFY(writeCredentials(path, 2'000'000'000'000));
    HttpServer server;
    server.enqueue({200, QByteArray(ClaudeProviderAdapter::MaximumResponseSize + 1, 'x')});
    QNetworkAccessManager network;
    ClaudeProviderAdapter adapter(&network);
    adapter.setCredentialPath(path);
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    adapter.setTimeoutMilliseconds(1000);
    QSignalSpy finished(&adapter, &ClaudeProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), QStringLiteral("Claude usage response exceeds the 1 MiB limit"));
}

QTEST_GUILESS_MAIN(ClaudeProviderAdapterTest)

#include "tst_claude_provider_adapter.moc"
