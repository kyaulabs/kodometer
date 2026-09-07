#include <kodometer/kimi_credentials.hpp>
#include <kodometer/kimi_provider_adapter.hpp>

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace Kodometer;

namespace {

struct HttpResponse
{
    int status = 200;
    QByteArray body;
    bool send = true;
};

class HttpServer final : public QTcpServer
{
  public:
    HttpServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                QTcpSocket *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    m_buffers[socket].append(socket->readAll());
                    if (!m_buffers[socket].contains("\r\n\r\n")) {
                        return;
                    }
                    const QByteArray request = m_buffers.take(socket);
                    m_requests.append(request);
                    if (m_responses.isEmpty()) {
                        return;
                    }
                    const HttpResponse response = m_responses.dequeue();
                    if (!response.send) {
                        return;
                    }
                    QByteArray reason = "OK";
                    if (response.status == 302) {
                        reason = "Found";
                    }
                    else if (response.status >= 400) {
                        reason = "Error";
                    }
                    const QByteArray output =
                        "HTTP/1.1 " + QByteArray::number(response.status) + ' ' + reason +
                        "\r\nContent-Type: application/json\r\nContent-Length: " +
                        QByteArray::number(response.body.size()) + "\r\nConnection: close\r\n\r\n" +
                        response.body;
                    socket->write(output);
                    socket->disconnectFromHost();
                });
            }
        });
        const bool listening = listen(QHostAddress::LocalHost);
        Q_ASSERT(listening);
    }

    void enqueue(HttpResponse response)
    {
        m_responses.enqueue(std::move(response));
    }

    [[nodiscard]] QUrl baseUrl() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(serverPort()));
    }

    [[nodiscard]] QList<QByteArray> requests() const
    {
        return m_requests;
    }

  private:
    QQueue<HttpResponse> m_responses;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QList<QByteArray> m_requests;
};

QByteArray validUsage()
{
    return R"({
      "usage":{"limit":"2048","used":"512","remaining":"1536",
               "resetTime":"2027-01-22T08:00:00Z"},
      "limits":[{"window":{"duration":300,"timeUnit":"TIME_UNIT_MINUTE"},
                 "detail":{"limit":"200","used":"50","remaining":"150",
                           "resetTime":"2027-01-15T13:00:00Z"}}]
    })";
}

QString writeCliCredential(const QString &home, double expiry)
{
    const QString credentials = home + QStringLiteral("/credentials");
    QDir().mkpath(credentials);
    const QString path = credentials + QStringLiteral("/kimi-code.json");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(QJsonDocument(QJsonObject{
                                 {QStringLiteral("access_token"), QStringLiteral("cli-token")},
                                 {QStringLiteral("refresh_token"), QStringLiteral("refresh")},
                                 {QStringLiteral("expires_at"), expiry},
                             })
                   .toJson(QJsonDocument::Compact));
    file.close();
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QString devicePath = home + QStringLiteral("/device_id");
    QFile device(devicePath);
    if (!device.open(QIODevice::WriteOnly)) {
        return {};
    }
    device.write("device-123\n");
    device.close();
    QFile::setPermissions(devicePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return path;
}

} // namespace

class KimiProviderAdapterTest final : public QObject
{
    Q_OBJECT

  private slots:
    void fetchesUsageWithApiKey();
    void namedAccountNeverFallsBackToCli();
    void reusesCliCredentialWithDeviceIdentity();
    void fallsBackToCliAfterRejectedApiKey();
    void reportsCredentialFailures();
    void classifiesHttpFailures_data();
    void classifiesHttpFailures();
    void normalizesUsageEndpoint_data();
    void normalizesUsageEndpoint();
    void reportsParseAndNetworkFailures();
    void boundsResponsesAndTimeouts();
    void ignoresConcurrentRefresh();
};

void KimiProviderAdapterTest::fetchesUsageWithApiKey()
{
    HttpServer server;
    server.enqueue({200, validUsage()});
    QNetworkAccessManager network;
    KimiProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({});
    adapter.setCredentialOverrides(
        {{QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("api-secret")}});
    adapter.setCurrentDateTime(QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC));
    QSignalSpy finished(&adapter, &KimiProviderAdapter::refreshFinished);
    QSignalSpy succeeded(&adapter, &ProviderAdapter::refreshSucceeded);

    QCOMPARE(adapter.providerId(), QStringLiteral("kimi"));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(succeeded.count(), 1);
    QVERIFY(!adapter.busy());
    QVERIFY(adapter.error().isEmpty());

    const QVariantMap provider = adapter.provider();
    QCOMPARE(provider.value(QStringLiteral("source")), QStringLiteral("api-key"));
    QCOMPARE(provider.value(QStringLiteral("windows")).toList().size(), 2);
    QCOMPARE(server.requests().size(), 1);
    const QByteArray request = server.requests().first();
    QVERIFY(request.startsWith("GET /coding/v1/usages HTTP/1.1\r\n"));
    QVERIFY(request.contains("Authorization: Bearer api-secret\r\n"));
    QVERIFY(request.contains("Accept: application/json\r\n"));
    QVERIFY(!request.contains("cli-token"));
}

void KimiProviderAdapterTest::reusesCliCredentialWithDeviceIdentity()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString home = temporary.path() + QStringLiteral("/kimi");
    QVERIFY(!writeCliCredential(home, 1'800'003'600.0).isEmpty());
    const QString credentialPath = home + QStringLiteral("/credentials/kimi-code.json");
    QFile credentialFile(credentialPath);
    QVERIFY(credentialFile.open(QIODevice::ReadOnly));
    const QByteArray original = credentialFile.readAll();
    credentialFile.close();

    HttpServer server;
    server.enqueue({200, validUsage()});
    server.enqueue({401, "{}"});
    QNetworkAccessManager network;
    KimiProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({{QStringLiteral("KIMI_CODE_HOME"), home}});
    adapter.setCurrentDateTime(QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC));
    QSignalSpy finished(&adapter, &KimiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(adapter.provider().value(QStringLiteral("source")), QStringLiteral("cli-oauth"));
    const QByteArray request = server.requests().first();
    QVERIFY(request.contains("Authorization: Bearer cli-token\r\n"));
    QVERIFY(request.contains("X-Msh-Platform: kimi_code_cli\r\n"));
    QVERIFY(request.contains("X-Msh-Device-Id: device-123\r\n"));
    QVERIFY(request.contains(QByteArrayLiteral("X-Msh-Version: " KODOMETER_VERSION "\r\n")));

    QVERIFY(credentialFile.open(QIODevice::ReadOnly));
    QCOMPARE(credentialFile.readAll(), original);
    credentialFile.close();

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).first().toBool(), false);
    QCOMPARE(adapter.error(),
             QStringLiteral("Kimi Code CLI credential is invalid or expired; sign in again or set "
                            "KIMI_CODE_API_KEY"));
}

void KimiProviderAdapterTest::fallsBackToCliAfterRejectedApiKey()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString home = temporary.path() + QStringLiteral("/kimi");
    QVERIFY(!writeCliCredential(home, 1'800'003'600.0).isEmpty());
    HttpServer server;
    server.enqueue({401, "{}"});
    server.enqueue({200, validUsage()});
    QNetworkAccessManager network;
    KimiProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({{QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("bad-key")},
                            {QStringLiteral("KIMI_CODE_HOME"), home}});
    adapter.setCurrentDateTime(QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC));
    QSignalSpy finished(&adapter, &KimiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(adapter.provider().value(QStringLiteral("source")), QStringLiteral("cli-oauth"));
    QCOMPARE(server.requests().size(), 2);
    QVERIFY(server.requests().at(0).contains("Authorization: Bearer bad-key\r\n"));
    QVERIFY(server.requests().at(1).contains("Authorization: Bearer cli-token\r\n"));
}

void KimiProviderAdapterTest::namedAccountNeverFallsBackToCli()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString home = temporary.path() + QStringLiteral("/kimi");
    QVERIFY(!writeCliCredential(home, 1'800'003'600.0).isEmpty());
    HttpServer server;
    server.enqueue({401, "{}"});
    server.enqueue({200, validUsage()});
    QNetworkAccessManager network;
    KimiProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({{"KIMI_CODE_API_KEY", "environment"}, {"KIMI_CODE_HOME", home}});
    adapter.setCurrentDateTime(QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC));
    QSignalSpy finished(&adapter, &KimiProviderAdapter::refreshFinished);
    adapter.setAccountCredential(QStringLiteral("named-wallet"));
    adapter.refresh();
    adapter.setAccountCredential(
        std::nullopt); // Closing/switching must not change this request's fallback policy.
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.last().first().toBool(), false);
    QCOMPARE(server.requests().size(), 1);
    QVERIFY(server.requests().first().contains("Authorization: Bearer named-wallet\r\n"));
    adapter.setAccountCredential(QString{});
    adapter.refresh();
    QCOMPARE(finished.count(), 2);
    QCOMPARE(server.requests().size(), 1);
    adapter.setAccountCredential(std::nullopt);
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QVERIFY(server.requests().last().contains("Authorization: Bearer environment\r\n"));
}

void KimiProviderAdapterTest::reportsCredentialFailures()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QNetworkAccessManager network;
    KimiProviderAdapter adapter(&network);
    adapter.setEnvironment(
        {{QStringLiteral("KIMI_CODE_HOME"), temporary.path() + QStringLiteral("/missing")}});
    QSignalSpy finished(&adapter, &KimiProviderAdapter::refreshFinished);
    QSignalSpy failed(&adapter, &ProviderAdapter::refreshFailed);

    adapter.refresh();
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("Kimi Code credentials were not found"));
    QVERIFY(adapter.provider().isEmpty());
}

void KimiProviderAdapterTest::classifiesHttpFailures_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QString>("message");
    QTest::newRow("bad-request") << 400 << QStringLiteral("Kimi Code usage request was rejected");
    QTest::newRow("unauthorized") << 401 << QStringLiteral("Kimi Code rejected the API key");
    QTest::newRow("forbidden") << 403 << QStringLiteral("Kimi Code denied access to usage quota");
    QTest::newRow("rate-limit") << 429
                                << QStringLiteral("Kimi Code usage request was rate limited");
    QTest::newRow("redirect") << 302
                              << QStringLiteral("Kimi Code usage request failed with HTTP 302");
    QTest::newRow("server") << 500
                            << QStringLiteral("Kimi Code usage request failed with HTTP 500");
}

void KimiProviderAdapterTest::classifiesHttpFailures()
{
    QFETCH(int, status);
    QFETCH(QString, message);
    HttpServer server;
    server.enqueue({status, "{}"});
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QNetworkAccessManager network;
    KimiProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment(
        {{QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("key")},
         {QStringLiteral("KIMI_CODE_HOME"), temporary.path() + QStringLiteral("/missing")}});
    QSignalSpy finished(&adapter, &KimiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), message);
    QCOMPARE(server.requests().size(), 1);
}

void KimiProviderAdapterTest::normalizesUsageEndpoint_data()
{
    QTest::addColumn<QString>("basePath");
    QTest::addColumn<QByteArray>("requestPath");
    QTest::newRow("root") << QString{} << QByteArray("/coding/v1/usages");
    QTest::newRow("coding") << QStringLiteral("/proxy/coding")
                            << QByteArray("/proxy/coding/v1/usages");
    QTest::newRow("coding-v1") << QStringLiteral("/proxy/coding/v1/")
                               << QByteArray("/proxy/coding/v1/usages");
}

void KimiProviderAdapterTest::normalizesUsageEndpoint()
{
    QFETCH(QString, basePath);
    QFETCH(QByteArray, requestPath);
    HttpServer server;
    server.enqueue({200, validUsage()});
    QUrl endpoint = server.baseUrl();
    endpoint.setPath(basePath);
    QNetworkAccessManager network;
    KimiProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(endpoint);
    adapter.setEnvironment({{QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("key")}});
    QSignalSpy finished(&adapter, &KimiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), true);
    QVERIFY(server.requests().first().startsWith("GET " + requestPath + " HTTP/1.1\r\n"));
}

void KimiProviderAdapterTest::reportsParseAndNetworkFailures()
{
    HttpServer server;
    server.enqueue({200, "{"});
    QNetworkAccessManager network;
    KimiProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({{QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("key")}});
    QSignalSpy finished(&adapter, &KimiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("Kimi Code usage API returned invalid JSON"));

    QTcpServer unused;
    QVERIFY(unused.listen(QHostAddress::LocalHost));
    const quint16 closedPort = unused.serverPort();
    unused.close();
    adapter.setBaseEndpoint(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(closedPort)));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QVERIFY(adapter.error().startsWith(QStringLiteral("Kimi Code usage request failed: network "
                                                      "error ")));
}

void KimiProviderAdapterTest::boundsResponsesAndTimeouts()
{
    HttpServer server;
    server.enqueue({200, QByteArray(KimiProviderAdapter::MaximumResponseSize + 1, 'x')});
    server.enqueue({200, {}, false});
    QNetworkAccessManager network;
    KimiProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({{QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("key")}});
    adapter.setTimeoutMilliseconds(40);
    QSignalSpy finished(&adapter, &KimiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("Kimi Code usage response exceeds the 1 MiB limit"));

    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 1'000);
    QCOMPARE(adapter.error(), QStringLiteral("Kimi Code usage request timed out"));
}

void KimiProviderAdapterTest::ignoresConcurrentRefresh()
{
    HttpServer server;
    server.enqueue({200, {}, false});
    QNetworkAccessManager network;
    KimiProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({{QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("key")}});
    adapter.setTimeoutMilliseconds(40);
    QSignalSpy finished(&adapter, &KimiProviderAdapter::refreshFinished);

    adapter.refresh();
    QVERIFY(adapter.busy());
    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1'000);
    QCOMPARE(server.requests().size(), 1);
}

QTEST_GUILESS_MAIN(KimiProviderAdapterTest)
#include "tst_kimi_provider_adapter.moc"
