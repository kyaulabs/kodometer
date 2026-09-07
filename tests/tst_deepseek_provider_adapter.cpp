#include <kodometer/deepseek_provider_adapter.hpp>

#include <QHash>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
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

QByteArray validBalance()
{
    return R"({"is_available":true,"balance_infos":[{
      "currency":"USD","total_balance":"50.00","granted_balance":"10.00",
      "topped_up_balance":"40.00"}]})";
}

} // namespace

class DeepSeekProviderAdapterTest final : public QObject
{
    Q_OBJECT

  private slots:
    void fetchesBalance();
    void namedAccountDoesNotUseEnvironmentOrAliases();
    void reportsCredentialFailures();
    void classifiesHttpFailures_data();
    void classifiesHttpFailures();
    void reportsParseAndNetworkFailures();
    void boundsResponsesAndTimeouts();
    void ignoresConcurrentRefreshAndRetainsLastGoodData();
};

void DeepSeekProviderAdapterTest::fetchesBalance()
{
    HttpServer server;
    server.enqueue({200, validBalance()});
    QNetworkAccessManager network;
    DeepSeekProviderAdapter adapter(&network);
    QUrl baseEndpoint = server.baseUrl();
    baseEndpoint.setPath(QStringLiteral("//"));
    adapter.setBaseEndpoint(baseEndpoint);
    adapter.setEnvironment({});
    adapter.setCredentialOverrides(
        {{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("api-secret")}});
    adapter.setCurrentDateTime(QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC));
    QSignalSpy finished(&adapter, &DeepSeekProviderAdapter::refreshFinished);
    QSignalSpy succeeded(&adapter, &ProviderAdapter::refreshSucceeded);

    QCOMPARE(adapter.providerId(), QStringLiteral("deepseek"));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(succeeded.count(), 1);
    QVERIFY(!adapter.busy());
    QVERIFY(adapter.error().isEmpty());
    QCOMPARE(adapter.provider()
                 .value(QStringLiteral("cost"))
                 .toMap()
                 .value(QStringLiteral("balance"))
                 .toDouble(),
             50.0);

    QCOMPARE(server.requests().size(), 1);
    const QByteArray request = server.requests().first();
    QVERIFY(request.startsWith("GET /user/balance HTTP/1.1\r\n"));
    QVERIFY(request.contains("Authorization: Bearer api-secret\r\n"));
    QVERIFY(request.contains("Accept: application/json\r\n"));
}

void DeepSeekProviderAdapterTest::namedAccountDoesNotUseEnvironmentOrAliases()
{
    HttpServer server;
    server.enqueue({200, validBalance()});
    server.enqueue({200, validBalance()});
    QNetworkAccessManager network;
    DeepSeekProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({{"DEEPSEEK_API_KEY", "environment"}, {"DEEPSEEK_KEY", "alias"}});
    adapter.setCredentialOverrides({{"DEEPSEEK_API_KEY", "default-wallet"}});
    QSignalSpy finished(&adapter, &DeepSeekProviderAdapter::refreshFinished);
    adapter.setAccountCredential(QStringLiteral("named-wallet"));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(server.requests().first().contains("Authorization: Bearer named-wallet\r\n"));
    adapter.setAccountCredential(QString{});
    adapter.refresh();
    QCOMPARE(finished.count(), 2);
    QCOMPARE(finished.last().first().toBool(), false);
    QCOMPARE(server.requests().size(), 1);
    adapter.setAccountCredential(std::nullopt);
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QVERIFY(server.requests().last().contains("Authorization: Bearer environment\r\n"));
}

void DeepSeekProviderAdapterTest::reportsCredentialFailures()
{
    QNetworkAccessManager network;
    DeepSeekProviderAdapter adapter(&network);
    adapter.setEnvironment({});
    QSignalSpy finished(&adapter, &DeepSeekProviderAdapter::refreshFinished);
    QSignalSpy failed(&adapter, &ProviderAdapter::refreshFailed);

    adapter.refresh();
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("DeepSeek API key is missing"));
    QVERIFY(adapter.provider().isEmpty());
}

void DeepSeekProviderAdapterTest::classifiesHttpFailures_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QString>("message");
    QTest::newRow("bad-request") << 400 << QStringLiteral("DeepSeek balance request was rejected");
    QTest::newRow("unauthorized") << 401 << QStringLiteral("DeepSeek rejected the API key");
    QTest::newRow("forbidden") << 403 << QStringLiteral("DeepSeek rejected the API key");
    QTest::newRow("rate-limit") << 429
                                << QStringLiteral("DeepSeek balance request was rate limited");
    QTest::newRow("redirect") << 302
                              << QStringLiteral("DeepSeek balance request failed with HTTP 302");
    QTest::newRow("server") << 500
                            << QStringLiteral("DeepSeek balance request failed with HTTP 500");
}

void DeepSeekProviderAdapterTest::classifiesHttpFailures()
{
    QFETCH(int, status);
    QFETCH(QString, message);
    HttpServer server;
    server.enqueue({status, "{}"});
    QNetworkAccessManager network;
    DeepSeekProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({{QStringLiteral("DEEPSEEK_KEY"), QStringLiteral("alias-key")}});
    QSignalSpy finished(&adapter, &DeepSeekProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), message);
    QCOMPARE(server.requests().size(), 1);
    QVERIFY(server.requests().first().contains("Authorization: Bearer alias-key\r\n"));
}

void DeepSeekProviderAdapterTest::reportsParseAndNetworkFailures()
{
    HttpServer server;
    server.enqueue({200, "{"});
    QNetworkAccessManager network;
    DeepSeekProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("key")}});
    QSignalSpy finished(&adapter, &DeepSeekProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("DeepSeek balance API returned invalid JSON"));

    QTcpServer unused;
    QVERIFY(unused.listen(QHostAddress::LocalHost));
    const quint16 closedPort = unused.serverPort();
    unused.close();
    adapter.setBaseEndpoint(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(closedPort)));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QVERIFY(adapter.error().startsWith(
        QStringLiteral("DeepSeek balance request failed: network error ")));
}

void DeepSeekProviderAdapterTest::boundsResponsesAndTimeouts()
{
    HttpServer server;
    server.enqueue({200, QByteArray(DeepSeekProviderAdapter::MaximumResponseSize + 1, 'x')});
    server.enqueue({200, {}, false});
    QNetworkAccessManager network;
    DeepSeekProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("key")}});
    adapter.setTimeoutMilliseconds(40);
    QSignalSpy finished(&adapter, &DeepSeekProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("DeepSeek balance response exceeds the 1 MiB limit"));

    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 1'000);
    QCOMPARE(adapter.error(), QStringLiteral("DeepSeek balance request timed out"));
}

void DeepSeekProviderAdapterTest::ignoresConcurrentRefreshAndRetainsLastGoodData()
{
    HttpServer server;
    server.enqueue({200, validBalance()});
    server.enqueue({200, {}, false});
    QNetworkAccessManager network;
    DeepSeekProviderAdapter adapter(&network);
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment({{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("key")}});
    adapter.setTimeoutMilliseconds(40);
    QSignalSpy finished(&adapter, &DeepSeekProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    const QVariantMap lastGood = adapter.provider();
    QVERIFY(!lastGood.isEmpty());

    adapter.refresh();
    QVERIFY(adapter.busy());
    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 1'000);
    QCOMPARE(server.requests().size(), 2);
    QCOMPARE(adapter.provider(), lastGood);
}

QTEST_GUILESS_MAIN(DeepSeekProviderAdapterTest)
#include "tst_deepseek_provider_adapter.moc"
