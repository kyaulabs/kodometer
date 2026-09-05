#include <kodometer/zai_provider_adapter.hpp>

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

    [[nodiscard]] QUrl endpoint(const QString &path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(path));
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

QByteArray quotaFixture()
{
    return R"({"code":200,"success":true,"data":{"planName":"Pro","limits":[
      {"type":"TOKENS_LIMIT","unit":3,"number":5,"percentage":25},
      {"type":"TOKENS_LIMIT","unit":6,"number":1,"percentage":9},
      {"type":"TIME_LIMIT","unit":5,"number":1,"percentage":22}
    ]}})";
}

QByteArray modelFixture()
{
    return R"({"code":200,"success":true,"data":{"x_time":["08:00"],
      "modelDataList":[{"modelName":"glm-4.6","tokensUsage":[100]}]}})";
}

QByteArray emptyModelFixture()
{
    return R"({"code":200,"success":true,"data":{"x_time":[],"modelDataList":[]}})";
}

QByteArray balanceFixture()
{
    return R"({"success":true,"data":{"availableBalance":42.5,"rechargeAmount":30,
      "giveAmount":12.5,"totalSpendAmount":8}})";
}

void configureEndpoints(ZaiProviderAdapter &adapter, const HttpServer &server, bool balance = false)
{
    adapter.setQuotaEndpoint(server.endpoint(QStringLiteral("/quota")));
    adapter.setModelUsageEndpoint(server.endpoint(QStringLiteral("/models")));
    if (balance) {
        adapter.setBalanceEndpoint(server.endpoint(QStringLiteral("/balance")));
    }
}

} // namespace

class ZaiProviderAdapterTest final : public QObject
{
    Q_OBJECT

  private slots:
    void fetchesGlobalQuotaAndModelUsage();
    void fetchesChinaTeamQuotaAndBalance();
    void preservesQuotaWhenOptionalRequestsFail();
    void reportsCredentialFailures();
    void classifiesQuotaHttpFailures_data();
    void classifiesQuotaHttpFailures();
    void reportsParseAndNetworkFailures();
    void boundsResponsesAndTimeouts();
    void ignoresConcurrentRefreshAndRetainsLastGoodData();
};

void ZaiProviderAdapterTest::fetchesGlobalQuotaAndModelUsage()
{
    HttpServer server;
    server.enqueue({200, quotaFixture()});
    server.enqueue({200, modelFixture()});
    server.enqueue({200, modelFixture()});
    QNetworkAccessManager network;
    ZaiProviderAdapter adapter(&network);
    configureEndpoints(adapter, server);
    adapter.setEnvironment({{QStringLiteral("Z_AI_API_KEY"), QStringLiteral("api-secret")}});
    adapter.setCurrentDateTime(QDateTime::fromSecsSinceEpoch(1'785'816'000, QTimeZone::UTC));
    QSignalSpy finished(&adapter, &ZaiProviderAdapter::refreshFinished);
    QSignalSpy succeeded(&adapter, &ProviderAdapter::refreshSucceeded);

    QCOMPARE(adapter.providerId(), QStringLiteral("zai"));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(succeeded.count(), 1);
    QVERIFY(adapter.error().isEmpty());
    QCOMPARE(adapter.provider().value(QStringLiteral("details")).toList().size(), 3);
    QCOMPARE(server.requests().size(), 3);
    QVERIFY(server.requests().at(0).startsWith("GET /quota HTTP/1.1\r\n"));
    QVERIFY(server.requests().at(0).contains("Authorization: Bearer api-secret\r\n"));
    QVERIFY(server.requests().at(1).startsWith("GET /models?"));
    QVERIFY(server.requests().at(1).contains("startTime="));
    QVERIFY(server.requests().at(1).contains("endTime="));
    QVERIFY(!server.requests().at(1).contains("type=3"));
}

void ZaiProviderAdapterTest::fetchesChinaTeamQuotaAndBalance()
{
    HttpServer server;
    server.enqueue({200, quotaFixture()});
    server.enqueue({200, emptyModelFixture()});
    server.enqueue({200, emptyModelFixture()});
    server.enqueue({200, balanceFixture()});
    QNetworkAccessManager network;
    ZaiProviderAdapter adapter(&network);
    configureEndpoints(adapter, server, true);
    adapter.setEnvironment({
        {QStringLiteral("Z_AI_REGION"), QStringLiteral("bigmodel-cn")},
        {QStringLiteral("Z_AI_USAGE_SCOPE"), QStringLiteral("team")},
        {QStringLiteral("BIGMODEL_API_KEY"), QStringLiteral("china-key")},
        {QStringLiteral("Z_AI_BIGMODEL_ORGANIZATION"), QStringLiteral("org-id")},
        {QStringLiteral("Z_AI_BIGMODEL_PROJECT"), QStringLiteral("project-id")},
    });
    QSignalSpy finished(&adapter, &ZaiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests().size(), 4);
    QVERIFY(server.requests().at(0).startsWith("GET /quota?type=2 HTTP/1.1\r\n"));
    QVERIFY(server.requests().at(0).contains("Bigmodel-Organization: org-id\r\n"));
    QVERIFY(server.requests().at(0).contains("Bigmodel-Project: project-id\r\n"));
    QVERIFY(server.requests().at(1).contains("type=3"));
    QVERIFY(server.requests().at(2).contains("type=3"));
    QVERIFY(server.requests().at(3).startsWith("GET /balance HTTP/1.1\r\n"));
    QVERIFY(!server.requests().at(3).contains("Bigmodel-Organization:"));

    const QVariantMap cost = adapter.provider().value(QStringLiteral("cost")).toMap();
    QCOMPARE(cost.value(QStringLiteral("balance")).toDouble(), 42.5);
    QCOMPARE(cost.value(QStringLiteral("currencyCode")), QStringLiteral("CNY"));
    QCOMPARE(cost.value(QStringLiteral("toppedUpBalance")).toDouble(), 30.0);
    QCOMPARE(cost.value(QStringLiteral("grantedBalance")).toDouble(), 12.5);
    QCOMPARE(cost.value(QStringLiteral("spent")).toDouble(), 8.0);
}

void ZaiProviderAdapterTest::preservesQuotaWhenOptionalRequestsFail()
{
    HttpServer server;
    server.enqueue({200, quotaFixture()});
    server.enqueue({500, "{}"});
    server.enqueue({200, "{"});
    server.enqueue({403, "{}"});
    QNetworkAccessManager network;
    ZaiProviderAdapter adapter(&network);
    configureEndpoints(adapter, server, true);
    adapter.setEnvironment({
        {QStringLiteral("Z_AI_REGION"), QStringLiteral("bigmodel-cn")},
        {QStringLiteral("Z_AI_API_KEY"), QStringLiteral("key")},
    });
    QSignalSpy finished(&adapter, &ZaiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), true);
    QVERIFY(adapter.error().isEmpty());
    QVERIFY(!adapter.provider().value(QStringLiteral("windows")).toList().isEmpty());
    QVERIFY(!adapter.provider().value(QStringLiteral("cost")).isValid());
}

void ZaiProviderAdapterTest::reportsCredentialFailures()
{
    QNetworkAccessManager network;
    ZaiProviderAdapter adapter(&network);
    adapter.setEnvironment({});
    adapter.setHomeDirectory(QStringLiteral("/nonexistent"));
    QSignalSpy finished(&adapter, &ZaiProviderAdapter::refreshFinished);
    QSignalSpy failed(&adapter, &ProviderAdapter::refreshFailed);

    adapter.refresh();
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("z.ai API key is missing"));
    QVERIFY(adapter.provider().isEmpty());
}

void ZaiProviderAdapterTest::classifiesQuotaHttpFailures_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QString>("message");
    QTest::newRow("bad-request") << 400 << QStringLiteral("z.ai quota request was rejected");
    QTest::newRow("unauthorized") << 401 << QStringLiteral("z.ai rejected the API key");
    QTest::newRow("forbidden") << 403 << QStringLiteral("z.ai rejected the API key");
    QTest::newRow("rate-limit") << 429 << QStringLiteral("z.ai quota request was rate limited");
    QTest::newRow("redirect") << 302 << QStringLiteral("z.ai quota request failed with HTTP 302");
    QTest::newRow("server") << 500 << QStringLiteral("z.ai quota request failed with HTTP 500");
}

void ZaiProviderAdapterTest::classifiesQuotaHttpFailures()
{
    QFETCH(int, status);
    QFETCH(QString, message);
    HttpServer server;
    server.enqueue({status, "{}"});
    QNetworkAccessManager network;
    ZaiProviderAdapter adapter(&network);
    configureEndpoints(adapter, server);
    adapter.setEnvironment({{QStringLiteral("Z_AI_API_KEY"), QStringLiteral("key")}});
    QSignalSpy finished(&adapter, &ZaiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), message);
}

void ZaiProviderAdapterTest::reportsParseAndNetworkFailures()
{
    HttpServer server;
    server.enqueue({200, "{"});
    QNetworkAccessManager network;
    ZaiProviderAdapter adapter(&network);
    configureEndpoints(adapter, server);
    adapter.setEnvironment({{QStringLiteral("Z_AI_API_KEY"), QStringLiteral("key")}});
    QSignalSpy finished(&adapter, &ZaiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("z.ai quota API returned invalid JSON"));

    QTcpServer unused;
    QVERIFY(unused.listen(QHostAddress::LocalHost));
    const quint16 closedPort = unused.serverPort();
    unused.close();
    adapter.setQuotaEndpoint(QUrl(QStringLiteral("http://127.0.0.1:%1/quota").arg(closedPort)));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QVERIFY(
        adapter.error().startsWith(QStringLiteral("z.ai quota request failed: network error ")));
}

void ZaiProviderAdapterTest::boundsResponsesAndTimeouts()
{
    HttpServer server;
    server.enqueue({200, QByteArray(ZaiProviderAdapter::MaximumResponseSize + 1, 'x')});
    server.enqueue({200, {}, false});
    QNetworkAccessManager network;
    ZaiProviderAdapter adapter(&network);
    configureEndpoints(adapter, server);
    adapter.setEnvironment({{QStringLiteral("Z_AI_API_KEY"), QStringLiteral("key")}});
    adapter.setTimeoutMilliseconds(40);
    QSignalSpy finished(&adapter, &ZaiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("z.ai quota response exceeds the 1 MiB limit"));

    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 1'000);
    QCOMPARE(adapter.error(), QStringLiteral("z.ai quota request timed out"));
}

void ZaiProviderAdapterTest::ignoresConcurrentRefreshAndRetainsLastGoodData()
{
    HttpServer server;
    server.enqueue({200, quotaFixture()});
    server.enqueue({200, emptyModelFixture()});
    server.enqueue({200, emptyModelFixture()});
    server.enqueue({200, {}, false});
    QNetworkAccessManager network;
    ZaiProviderAdapter adapter(&network);
    configureEndpoints(adapter, server);
    adapter.setEnvironment({{QStringLiteral("Z_AI_API_KEY"), QStringLiteral("key")}});
    adapter.setTimeoutMilliseconds(40);
    QSignalSpy finished(&adapter, &ZaiProviderAdapter::refreshFinished);

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    const QVariantMap lastGood = adapter.provider();
    QVERIFY(!lastGood.isEmpty());

    adapter.refresh();
    QVERIFY(adapter.busy());
    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 1'000);
    QCOMPARE(server.requests().size(), 4);
    QCOMPARE(adapter.provider(), lastGood);
}

QTEST_GUILESS_MAIN(ZaiProviderAdapterTest)
#include "tst_zai_provider_adapter.moc"
