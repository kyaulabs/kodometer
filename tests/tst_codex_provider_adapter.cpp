#include <kodometer/codex_provider_adapter.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

using Kodometer::CodexProviderAdapter;

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
        const bool listening = m_server.listen(QHostAddress::LocalHost);
        Q_ASSERT(listening);
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
        if (lines.isEmpty()) {
            return;
        }
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

QString jwt(const QJsonObject &claims)
{
    const auto encode = [](const QByteArray &value) {
        return QString::fromLatin1(
            value.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    };
    return encode("{\"alg\":\"none\"}") + QLatin1Char('.') +
           encode(QJsonDocument(claims).toJson(QJsonDocument::Compact)) +
           QStringLiteral(".signature");
}

bool writeCredentials(const QString &path, const QString &accessToken,
                      const QString &refreshToken = QStringLiteral("refresh"))
{
    const QJsonObject root{
        {QStringLiteral("tokens"),
         QJsonObject{
             {QStringLiteral("access_token"), accessToken},
             {QStringLiteral("refresh_token"), refreshToken},
             {QStringLiteral("account_id"), QStringLiteral("account-1")},
         }},
    };
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson());
    file.close();
    return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QByteArray usagePayload()
{
    return R"({"plan_type":"plus","rate_limit":{"primary_window":{"used_percent":25,"reset_at":1800000000,"limit_window_seconds":18000}}})";
}

} // namespace

class CodexProviderAdapterTest final : public QObject
{
    Q_OBJECT

  private slots:
    void fetchesUsageWithExistingToken();
    void refreshesExpiringTokenBeforeUsage();
    void retriesUnauthorizedUsageOnce();
    void supportsApiKeyWithoutAccount();
    void reportsCredentialAndResponseFailures();
    void reportsTokenFailures();
    void reportsNetworkAndRedirectFailures();
    void rejectsConcurrentRefreshAndTimesOut();
    void limitsResponseSize();
};

void CodexProviderAdapterTest::fetchesUsageWithExistingToken()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString credentialPath = directory.filePath(QStringLiteral("auth.json"));
    const QString accessToken = jwt({{QStringLiteral("exp"), 2'000'000'000}});
    QVERIFY(writeCredentials(credentialPath, accessToken));
    HttpServer server;
    server.enqueue({200, usagePayload()});
    QNetworkAccessManager network;
    CodexProviderAdapter adapter(&network);
    adapter.setCredentialPath(credentialPath);
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    adapter.setTokenEndpoint(server.url(QStringLiteral("/token")));
    QSignalSpy finished(&adapter, &CodexProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 1);
    QCOMPARE(server.requests.first().method, QByteArray("GET"));
    QCOMPARE(server.requests.first().path, QByteArray("/usage"));
    QCOMPARE(server.requests.first().headers.value("authorization"),
             QByteArray("Bearer ") + accessToken.toUtf8());
    QCOMPARE(server.requests.first().headers.value("chatgpt-account-id"), QByteArray("account-1"));
    QCOMPARE(server.requests.first().headers.value("accept"), QByteArray("application/json"));
    QCOMPARE(adapter.provider().value(QStringLiteral("id")).toString(), QStringLiteral("codex"));
    QVERIFY(adapter.error().isEmpty());
    QVERIFY(!adapter.busy());
}

void CodexProviderAdapterTest::refreshesExpiringTokenBeforeUsage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString credentialPath = directory.filePath(QStringLiteral("auth.json"));
    const QString expired = jwt({{QStringLiteral("exp"), 1}});
    QVERIFY(writeCredentials(credentialPath, expired));
    HttpServer server;
    server.enqueue(
        {200,
         R"({"access_token":"fresh-access","refresh_token":"fresh-refresh","id_token":"fresh-id"})"});
    server.enqueue({200, usagePayload()});
    QNetworkAccessManager network;
    CodexProviderAdapter adapter(&network);
    adapter.setCredentialPath(credentialPath);
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    adapter.setTokenEndpoint(server.url(QStringLiteral("/token")));
    QSignalSpy finished(&adapter, &CodexProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 2);
    QCOMPARE(server.requests.at(0).method, QByteArray("POST"));
    QCOMPARE(server.requests.at(0).path, QByteArray("/token"));
    const QJsonObject refreshBody = QJsonDocument::fromJson(server.requests.at(0).body).object();
    QCOMPARE(refreshBody.value(QStringLiteral("grant_type")).toString(),
             QStringLiteral("refresh_token"));
    QCOMPARE(refreshBody.value(QStringLiteral("refresh_token")).toString(),
             QStringLiteral("refresh"));
    QCOMPARE(server.requests.at(1).headers.value("authorization"),
             QByteArray("Bearer fresh-access"));

    QFile file(credentialPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject savedTokens =
        QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("tokens")).toObject();
    QCOMPARE(savedTokens.value(QStringLiteral("access_token")).toString(),
             QStringLiteral("fresh-access"));
    QCOMPARE(savedTokens.value(QStringLiteral("refresh_token")).toString(),
             QStringLiteral("fresh-refresh"));
}

void CodexProviderAdapterTest::retriesUnauthorizedUsageOnce()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString credentialPath = directory.filePath(QStringLiteral("auth.json"));
    const QString current = jwt({{QStringLiteral("exp"), 2'000'000'000}});
    QVERIFY(writeCredentials(credentialPath, current));
    HttpServer server;
    server.enqueue({401, R"({"error":"expired"})"});
    server.enqueue({200, R"({"access_token":"retry-access"})"});
    server.enqueue({200, usagePayload()});
    QNetworkAccessManager network;
    CodexProviderAdapter adapter(&network);
    adapter.setCredentialPath(credentialPath);
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    adapter.setTokenEndpoint(server.url(QStringLiteral("/token")));
    QSignalSpy finished(&adapter, &CodexProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 3);
    QCOMPARE(server.requests.at(0).path, QByteArray("/usage"));
    QCOMPARE(server.requests.at(1).path, QByteArray("/token"));
    QCOMPARE(server.requests.at(2).path, QByteArray("/usage"));
}

void CodexProviderAdapterTest::supportsApiKeyWithoutAccount()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString credentialPath = directory.filePath(QStringLiteral("auth.json"));
    QFile file(credentialPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"OPENAI_API_KEY":"sk-example"})");
    file.close();
    QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    HttpServer server;
    server.enqueue({200, usagePayload()});
    QNetworkAccessManager network;
    CodexProviderAdapter adapter(&network);
    adapter.setCredentialPath(credentialPath);
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    QSignalSpy finished(&adapter, &CodexProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QVERIFY(!server.requests.first().headers.contains("chatgpt-account-id"));
    QCOMPARE(adapter.provider().value(QStringLiteral("source")).toString(),
             QStringLiteral("api-key"));
}

void CodexProviderAdapterTest::reportsCredentialAndResponseFailures()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QNetworkAccessManager network;
    CodexProviderAdapter adapter(&network);
    adapter.setCredentialPath(directory.filePath(QStringLiteral("missing.json")));
    QSignalSpy finished(&adapter, &CodexProviderAdapter::refreshFinished);

    adapter.refresh();
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), QStringLiteral("Codex auth file was not found"));

    const QString credentialPath = directory.filePath(QStringLiteral("auth.json"));
    QVERIFY(writeCredentials(credentialPath, jwt({{QStringLiteral("exp"), 2'000'000'000}})));
    HttpServer server;
    server.enqueue({500, "failure"});
    adapter.setCredentialPath(credentialPath);
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    adapter.setTokenEndpoint(server.url(QStringLiteral("/token")));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).first().toBool(), false);
    QCOMPARE(adapter.error(), QStringLiteral("Codex usage request failed with HTTP 500"));

    server.enqueue({200, "{"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(adapter.error(), QStringLiteral("Codex usage API returned invalid JSON"));

    server.enqueue({200, usagePayload()});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 4);
    QVERIFY(adapter.error().isEmpty());
}

void CodexProviderAdapterTest::reportsTokenFailures()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString credentialPath = directory.filePath(QStringLiteral("auth.json"));
    QVERIFY(writeCredentials(credentialPath, jwt({{QStringLiteral("exp"), 1}})));
    HttpServer server;
    QNetworkAccessManager network;
    CodexProviderAdapter adapter(&network);
    adapter.setCredentialPath(credentialPath);
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    adapter.setTokenEndpoint(server.url(QStringLiteral("/token")));
    adapter.setTimeoutMilliseconds(1000);
    QSignalSpy finished(&adapter, &CodexProviderAdapter::refreshFinished);

    server.enqueue({400, R"({"error":"invalid_grant"})"});
    adapter.refresh();
    QVERIFY(finished.wait());
    QCOMPARE(adapter.error(), QStringLiteral("Codex token request failed with HTTP 400"));

    server.enqueue({200, "{"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(adapter.error(), QStringLiteral("Codex token endpoint returned invalid JSON"));

    server.enqueue({200, "{}", {}, false});
    adapter.setTimeoutMilliseconds(25);
    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 3, 1000);
    QCOMPARE(adapter.error(), QStringLiteral("Codex token request timed out"));

    adapter.setTimeoutMilliseconds(1000);
    server.enqueue({200, QByteArray(CodexProviderAdapter::MaximumResponseSize + 1, 'x')});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 4);
    QCOMPARE(adapter.error(), QStringLiteral("Codex token response exceeds the 1 MiB limit"));

    server.enqueue({200, R"({"access_token":"fresh"})"});
    server.enqueue({401, "{}"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 5);
    QCOMPARE(adapter.error(), QStringLiteral("Codex usage request failed with HTTP 401"));
}

void CodexProviderAdapterTest::reportsNetworkAndRedirectFailures()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString credentialPath = directory.filePath(QStringLiteral("auth.json"));
    QVERIFY(writeCredentials(credentialPath, jwt({{QStringLiteral("exp"), 2'000'000'000}})));
    QNetworkAccessManager network;
    CodexProviderAdapter adapter(&network);
    adapter.setCredentialPath(credentialPath);
    adapter.setUsageEndpoint(QUrl(QStringLiteral("http://127.0.0.1:1/usage")));
    QSignalSpy finished(&adapter, &CodexProviderAdapter::refreshFinished);

    adapter.refresh();
    QVERIFY(finished.wait());
    QVERIFY(
        adapter.error().startsWith(QStringLiteral("Codex usage request failed: network error")));

    HttpServer server;
    server.enqueue({302, "", {{QByteArray("Location"), QByteArray("/elsewhere")}}});
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(adapter.error(), QStringLiteral("Codex usage request failed with HTTP 302"));

    QVERIFY(writeCredentials(credentialPath, jwt({{QStringLiteral("exp"), 1}})));
    adapter.setTokenEndpoint(QUrl(QStringLiteral("http://127.0.0.1:1/token")));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QVERIFY(
        adapter.error().startsWith(QStringLiteral("Codex token request failed: network error")));
}

void CodexProviderAdapterTest::rejectsConcurrentRefreshAndTimesOut()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString credentialPath = directory.filePath(QStringLiteral("auth.json"));
    QVERIFY(writeCredentials(credentialPath, jwt({{QStringLiteral("exp"), 2'000'000'000}})));
    HttpServer server;
    server.enqueue({200, "{}", {}, false});
    QNetworkAccessManager network;
    CodexProviderAdapter adapter(&network);
    adapter.setCredentialPath(credentialPath);
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    adapter.setTimeoutMilliseconds(25);
    QSignalSpy finished(&adapter, &CodexProviderAdapter::refreshFinished);

    adapter.refresh();
    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(server.requests.size(), 1);
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), QStringLiteral("Codex usage request timed out"));
}

void CodexProviderAdapterTest::limitsResponseSize()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString credentialPath = directory.filePath(QStringLiteral("auth.json"));
    QVERIFY(writeCredentials(credentialPath, jwt({{QStringLiteral("exp"), 2'000'000'000}})));
    HttpServer server;
    server.enqueue({200, QByteArray(CodexProviderAdapter::MaximumResponseSize + 1, 'x')});
    QNetworkAccessManager network;
    CodexProviderAdapter adapter(&network);
    adapter.setCredentialPath(credentialPath);
    adapter.setUsageEndpoint(server.url(QStringLiteral("/usage")));
    QSignalSpy finished(&adapter, &CodexProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), QStringLiteral("Codex usage response exceeds the 1 MiB limit"));
}

QTEST_GUILESS_MAIN(CodexProviderAdapterTest)

#include "tst_codex_provider_adapter.moc"
