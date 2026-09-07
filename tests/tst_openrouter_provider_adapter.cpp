#include <kodometer/openrouter_provider_adapter.hpp>

#include <QNetworkAccessManager>
#include <QQueue>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimeZone>
#include <QtTest>

using Kodometer::OpenRouterProviderAdapter;

namespace {

struct HttpRequest
{
    QByteArray method;
    QByteArray path;
    QMap<QByteArray, QByteArray> headers;
};

struct HttpResponse
{
    HttpResponse(int responseStatus = 200, QByteArray responseBody = "{}", bool shouldSend = true)
        : status(responseStatus), body(std::move(responseBody)), send(shouldSend)
    {}
    int status;
    QByteArray body;
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
                    const QByteArray data = socket->readAll();
                    const qsizetype headerEnd = data.indexOf("\r\n\r\n");
                    if (headerEnd < 0) {
                        return;
                    }
                    const QList<QByteArray> lines = data.left(headerEnd).split('\n');
                    const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
                    HttpRequest request{requestLine.at(0), requestLine.at(1), {}};
                    for (qsizetype index = 1; index < lines.size(); ++index) {
                        const QByteArray line = lines.at(index).trimmed();
                        const qsizetype separator = line.indexOf(':');
                        if (separator > 0) {
                            request.headers.insert(line.left(separator).toLower(),
                                                   line.mid(separator + 1).trimmed());
                        }
                    }
                    requests.append(request);
                    if (m_responses.isEmpty()) {
                        return;
                    }
                    const HttpResponse response = m_responses.dequeue();
                    if (!response.send) {
                        return;
                    }
                    const QByteArray reason = response.status >= 400 ? "Error" : "OK";
                    QByteArray output = "HTTP/1.1 " + QByteArray::number(response.status) + ' ' +
                                        reason + "\r\nContent-Type: application/json\r\n";
                    output += "Content-Length: " + QByteArray::number(response.body.size()) +
                              "\r\nConnection: close\r\n\r\n" + response.body;
                    socket->write(output);
                    socket->disconnectFromHost();
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

    [[nodiscard]] QUrl apiUrl() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1").arg(m_server.serverPort()));
    }

    QList<HttpRequest> requests;

  private:
    QTcpServer m_server;
    QQueue<HttpResponse> m_responses;
};

QMap<QString, QString> credentials(bool management = false)
{
    QMap<QString, QString> result{
        {QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("user-key")},
        {QStringLiteral("OPENROUTER_HTTP_REFERER"), QStringLiteral("https://kodometer.test")},
        {QStringLiteral("OPENROUTER_X_TITLE"), QStringLiteral("Kodometer Test")},
    };
    if (management) {
        result.insert(QStringLiteral("OPENROUTER_MANAGEMENT_API_KEY"),
                      QStringLiteral("management-key"));
    }
    return result;
}

QByteArray credits()
{
    return R"({"data":{"total_credits":100,"total_usage":40}})";
}

QByteArray keyUsage()
{
    return R"({"data":{"limit":20,"limit_remaining":15,"limit_reset":"monthly","usage":5,"usage_daily":1,"usage_weekly":2,"usage_monthly":4}})";
}

void configure(OpenRouterProviderAdapter &adapter, const HttpServer &server,
               bool management = false)
{
    adapter.setApiBaseEndpoint(server.apiUrl());
    adapter.setActivityEndpoint(QUrl(server.apiUrl().toString() + QStringLiteral("/activity")));
    adapter.setEnvironment(credentials(management));
    adapter.setCurrentDateTime(QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC));
}

} // namespace

class OpenRouterProviderAdapterTest final : public QObject
{
    Q_OBJECT

  private slots:
    void fetchesCreditsKeyQuotaAndActivity();
    void isolatesNamedKeyPairs();
    void preservesNamedCreditsAfterManagementRejection();
    void preservesCreditsWhenOptionalRequestsFail();
    void reportsCredentialFailures();
    void classifiesCreditsHttpFailures_data();
    void classifiesCreditsHttpFailures();
    void reportsParseAndNetworkFailures();
    void boundsResponsesAndTimeouts();
    void ignoresConcurrentRefreshAndRetainsLastGoodData();
};

void OpenRouterProviderAdapterTest::fetchesCreditsKeyQuotaAndActivity()
{
    HttpServer server;
    server.enqueue({200, credits()});
    server.enqueue({200, keyUsage()});
    server.enqueue(
        {200,
         R"({"data":[{"date":"2027-01-13","prompt_tokens":2,"completion_tokens":1,"requests":1,"usage":0.5}]})"});
    server.enqueue(
        {200,
         R"({"data":[{"date":"2027-01-14","prompt_tokens":3,"completion_tokens":2,"requests":1,"usage":1.5}]})"});
    QNetworkAccessManager network;
    OpenRouterProviderAdapter adapter(&network);
    configure(adapter, server, true);
    QUrl apiEndpoint = server.apiUrl();
    apiEndpoint.setPath(apiEndpoint.path() + QStringLiteral("//"));
    adapter.setApiBaseEndpoint(apiEndpoint);
    adapter.setEnvironment(
        {{QStringLiteral("OPENROUTER_HTTP_REFERER"), QStringLiteral("https://kodometer.test")},
         {QStringLiteral("OPENROUTER_X_TITLE"), QStringLiteral("Kodometer Test")}});
    adapter.setCredentialOverrides(
        {{QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("user-key")},
         {QStringLiteral("OPENROUTER_MANAGEMENT_API_KEY"), QStringLiteral("management-key")}});
    QSignalSpy finished(&adapter, &OpenRouterProviderAdapter::refreshFinished);

    adapter.refresh();
    // All Activity requests and parsing use the same UTC calendar window.
    adapter.setCurrentDateTime(QDateTime::fromSecsSinceEpoch(1'800'086'400, QTimeZone::UTC));

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(adapter.provider().value(QStringLiteral("updatedAt")).toString(),
             QStringLiteral("2027-01-15T08:00:00.000Z"));
    QCOMPARE(adapter.provider()
                 .value(QStringLiteral("cost"))
                 .toMap()
                 .value(QStringLiteral("historyEndDate"))
                 .toString(),
             QStringLiteral("2027-01-14"));
    QCOMPARE(server.requests.size(), 4);
    QCOMPARE(server.requests.at(0).path, QByteArray("/api/v1/credits"));
    QCOMPARE(server.requests.at(1).path, QByteArray("/api/v1/key"));
    QCOMPARE(server.requests.at(2).path, QByteArray("/api/v1/activity"));
    QCOMPARE(server.requests.at(3).path, QByteArray("/api/v1/activity?date=2027-01-14"));
    QCOMPARE(server.requests.at(0).headers.value("authorization"), QByteArray("Bearer user-key"));
    QCOMPARE(server.requests.at(0).headers.value("http-referer"),
             QByteArray("https://kodometer.test"));
    QCOMPARE(server.requests.at(0).headers.value("x-title"), QByteArray("Kodometer Test"));
    QCOMPARE(server.requests.at(2).headers.value("authorization"),
             QByteArray("Bearer management-key"));
    QCOMPARE(adapter.provider().value(QStringLiteral("id")), QStringLiteral("openrouter"));
    QCOMPARE(adapter.provider()
                 .value(QStringLiteral("cost"))
                 .toMap()
                 .value(QStringLiteral("last30DaysUSD"))
                 .toDouble(),
             2.0);
    QVERIFY(adapter.error().isEmpty());
    QVERIFY(!adapter.busy());
}

void OpenRouterProviderAdapterTest::isolatesNamedKeyPairs()
{
    HttpServer server;
    for (int cycle = 0; cycle < 3; ++cycle) {
        server.enqueue({200, credits()});
        server.enqueue({200, keyUsage()});
        if (cycle != 1) {
            server.enqueue({200, R"({"data":[]})"});
            server.enqueue({200, R"({"data":[]})"});
        }
    }
    QNetworkAccessManager network;
    OpenRouterProviderAdapter adapter(&network);
    configure(adapter, server, true);
    adapter.setCredentialOverrides({{"OPENROUTER_API_KEY", "default-ordinary"},
                                    {"OPENROUTER_MANAGEMENT_API_KEY", "default-management"}});
    QSignalSpy finished(&adapter, &OpenRouterProviderAdapter::refreshFinished);
    adapter.setAccountCredential(QStringLiteral("named-ordinary"),
                                 QStringLiteral("named-management"));
    adapter.refresh();
    adapter.setAccountCredential(QStringLiteral("next-ordinary"));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(server.requests.size(), 4);
    QCOMPARE(server.requests.at(0).headers.value("authorization"),
             QByteArray("Bearer named-ordinary"));
    QCOMPARE(server.requests.at(1).headers.value("authorization"),
             QByteArray("Bearer named-ordinary"));
    QCOMPARE(server.requests.at(2).headers.value("authorization"),
             QByteArray("Bearer named-management"));
    QCOMPARE(server.requests.at(3).headers.value("authorization"),
             QByteArray("Bearer named-management"));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(server.requests.size(), 6); // No borrowed Management key or Activity requests.
    QCOMPARE(server.requests.at(4).headers.value("authorization"),
             QByteArray("Bearer next-ordinary"));
    adapter.setAccountCredential(QString{}, QStringLiteral("only-management"));
    adapter.refresh();
    QCOMPARE(finished.count(), 3);
    QCOMPARE(finished.last().first().toBool(), false);
    QCOMPARE(server.requests.size(), 6);
    adapter.setAccountCredential(std::nullopt);
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 4);
    QCOMPARE(server.requests.size(), 10);
    QCOMPARE(server.requests.at(6).headers.value("authorization"), QByteArray("Bearer user-key"));
    QCOMPARE(server.requests.at(8).headers.value("authorization"),
             QByteArray("Bearer management-key"));
}

void OpenRouterProviderAdapterTest::preservesNamedCreditsAfterManagementRejection()
{
    HttpServer server;
    server.enqueue({200, credits()});
    server.enqueue({200, keyUsage()});
    server.enqueue({403, "{}"});
    QNetworkAccessManager network;
    OpenRouterProviderAdapter adapter(&network);
    configure(adapter, server, true);
    adapter.setAccountCredential(QStringLiteral("named"), QStringLiteral("rejected-management"));
    QSignalSpy finished(&adapter, &OpenRouterProviderAdapter::refreshFinished);
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), true);
    QCOMPARE(server.requests.size(), 3);
    QCOMPARE(server.requests.last().headers.value("authorization"),
             QByteArray("Bearer rejected-management"));
    QCOMPARE(adapter.provider().value("cost").toMap().value("balanceUSD").toDouble(), 60.0);
}

void OpenRouterProviderAdapterTest::preservesCreditsWhenOptionalRequestsFail()
{
    HttpServer server;
    server.enqueue({200, credits()});
    server.enqueue({500, "{}"});
    server.enqueue({200, credits()});
    server.enqueue({200, "{"});
    server.enqueue({500, "{}"});
    QNetworkAccessManager network;
    OpenRouterProviderAdapter adapter(&network);
    configure(adapter, server);
    QSignalSpy finished(&adapter, &OpenRouterProviderAdapter::refreshFinished);

    adapter.refresh();
    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QVERIFY(adapter.provider().value(QStringLiteral("windows")).toList().isEmpty());
    QCOMPARE(adapter.provider().value(QStringLiteral("cost")).toMap().value("balanceUSD"), 60.0);

    adapter.setEnvironment(credentials(true));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).first().toBool(), true);
    QVERIFY(!adapter.provider().value(QStringLiteral("cost")).toMap().contains("last30DaysUSD"));

    server.enqueue({200, credits()});
    server.enqueue({200, keyUsage()});
    server.enqueue(
        {200,
         R"({"data":[{"date":"2027-01-13","prompt_tokens":2,"completion_tokens":1,"requests":1,"usage":0.5}]})"});
    server.enqueue({500, "{}"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(finished.at(2).first().toBool(), true);
    QVERIFY(!adapter.provider().value(QStringLiteral("cost")).toMap().contains("last30DaysUSD"));
}

void OpenRouterProviderAdapterTest::reportsCredentialFailures()
{
    QNetworkAccessManager network;
    OpenRouterProviderAdapter adapter(&network);
    QSignalSpy finished(&adapter, &OpenRouterProviderAdapter::refreshFinished);

    adapter.setEnvironment({});
    adapter.refresh();

    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), QStringLiteral("OpenRouter API key is missing"));
}

void OpenRouterProviderAdapterTest::classifiesCreditsHttpFailures_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QString>("message");
    QTest::newRow("unauthorized") << 401 << QStringLiteral("OpenRouter rejected the API key");
    QTest::newRow("forbidden") << 403 << QStringLiteral("OpenRouter rejected the API key");
    QTest::newRow("limited") << 429
                             << QStringLiteral("OpenRouter credits request was rate limited");
    QTest::newRow("server") << 503
                            << QStringLiteral("OpenRouter credits request failed with HTTP 503");
}

void OpenRouterProviderAdapterTest::classifiesCreditsHttpFailures()
{
    QFETCH(int, status);
    QFETCH(QString, message);
    HttpServer server;
    server.enqueue({status, "{}"});
    QNetworkAccessManager network;
    OpenRouterProviderAdapter adapter(&network);
    configure(adapter, server);
    QSignalSpy finished(&adapter, &OpenRouterProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), message);
}

void OpenRouterProviderAdapterTest::reportsParseAndNetworkFailures()
{
    HttpServer server;
    server.enqueue({200, "{"});
    QNetworkAccessManager network;
    OpenRouterProviderAdapter adapter(&network);
    configure(adapter, server);
    QSignalSpy finished(&adapter, &OpenRouterProviderAdapter::refreshFinished);

    adapter.refresh();
    QVERIFY(finished.wait());
    QCOMPARE(adapter.error(), QStringLiteral("OpenRouter credits API returned invalid JSON"));

    adapter.setApiBaseEndpoint(QUrl(QStringLiteral("http://127.0.0.1:1/api/v1")));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QVERIFY(adapter.error().startsWith(
        QStringLiteral("OpenRouter credits request failed: network error")));
}

void OpenRouterProviderAdapterTest::boundsResponsesAndTimeouts()
{
    HttpServer server;
    QNetworkAccessManager network;
    OpenRouterProviderAdapter adapter(&network);
    configure(adapter, server);
    adapter.setTimeoutMilliseconds(20);
    QSignalSpy finished(&adapter, &OpenRouterProviderAdapter::refreshFinished);

    server.enqueue({200, "{}", false});
    adapter.refresh();
    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);
    QCOMPARE(server.requests.size(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("OpenRouter credits request timed out"));

    adapter.setTimeoutMilliseconds(1000);
    server.enqueue({200, QByteArray(OpenRouterProviderAdapter::MaximumResponseSize + 1, 'x')});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(adapter.error(),
             QStringLiteral("OpenRouter credits response exceeds the 1 MiB limit"));

    server.enqueue({200, credits()});
    server.enqueue({200, QByteArray(OpenRouterProviderAdapter::MaximumResponseSize + 1, 'x')});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(finished.at(2).first().toBool(), true);

    adapter.setTimeoutMilliseconds(20);
    server.enqueue({200, credits()});
    server.enqueue({200, "{}", false});
    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 4, 1000);
    QCOMPARE(finished.at(3).first().toBool(), true);

    adapter.setEnvironment(credentials(true));
    adapter.setTimeoutMilliseconds(1000);
    server.enqueue({200, credits()});
    server.enqueue({200, keyUsage()});
    server.enqueue({200, QByteArray(OpenRouterProviderAdapter::MaximumResponseSize + 1, 'x')});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 5);
    QCOMPARE(finished.at(4).first().toBool(), true);

    server.enqueue({200, credits()});
    server.enqueue({200, keyUsage()});
    server.enqueue({403, "{}"});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 6);
    QCOMPARE(finished.at(5).first().toBool(), true);
    const QVariantList sections = adapter.provider().value("details").toList();
    QCOMPARE(sections.last().toMap().value("rows").toList().first().toMap().value("secondaryValue"),
             QStringLiteral("Management API key required"));

    adapter.setTimeoutMilliseconds(20);
    server.enqueue({200, credits()});
    server.enqueue({200, keyUsage()});
    server.enqueue({200, "{}", false});
    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 7, 1000);
    QCOMPARE(finished.at(6).first().toBool(), true);
}

void OpenRouterProviderAdapterTest::ignoresConcurrentRefreshAndRetainsLastGoodData()
{
    HttpServer server;
    server.enqueue({200, credits()});
    server.enqueue({200, keyUsage()});
    server.enqueue({500, "{}"});
    QNetworkAccessManager network;
    OpenRouterProviderAdapter adapter(&network);
    configure(adapter, server);
    QSignalSpy finished(&adapter, &OpenRouterProviderAdapter::refreshFinished);

    adapter.refresh();
    adapter.refresh();
    QVERIFY(finished.wait());
    QCOMPARE(server.requests.size(), 2);
    const QVariantMap lastGood = adapter.provider();

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).first().toBool(), false);
    QCOMPARE(adapter.provider(), lastGood);
}

QTEST_GUILESS_MAIN(OpenRouterProviderAdapterTest)

#include "tst_openrouter_provider_adapter.moc"
