#include <kodometer/xai_provider_adapter.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QQueue>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimeZone>
#include <QtTest>

using Kodometer::XaiProviderAdapter;

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

    [[nodiscard]] QUrl baseUrl() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort()));
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
        output += "Connection: close\r\n\r\n" + response.body;
        socket->write(output);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QQueue<HttpResponse> m_responses;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

QMap<QString, QString> credentials()
{
    return {{QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("management-key")},
            {QStringLiteral("XAI_TEAM_ID"), QStringLiteral("team-1234")}};
}

QByteArray usagePayload()
{
    return R"({"timeSeries":[{"dataPoints":[{"timestamp":"2027-01-15T00:00:00Z","values":[1.25]}]}],"limitReached":false})";
}

void configure(XaiProviderAdapter &adapter, const HttpServer &server)
{
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.setEnvironment(credentials());
    adapter.setCurrentDateTime(QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC));
}

} // namespace

class XaiProviderAdapterTest final : public QObject
{
    Q_OBJECT

  private slots:
    void fetchesBalanceAndDailyUsage();
    void isolatesNamedKeyAndTeam();
    void preservesBalanceWhenHistoryIsUnavailable();
    void reportsCredentialFailures();
    void classifiesBalanceHttpFailures();
    void reportsBalanceParseAndNetworkFailures();
    void rejectsUnauthorizedHistory();
    void boundsResponsesAndTimeouts();
};

void XaiProviderAdapterTest::fetchesBalanceAndDailyUsage()
{
    HttpServer server;
    server.enqueue({200, R"({"total":{"val":"-1000"}})"});
    server.enqueue({200, usagePayload()});
    QNetworkAccessManager network;
    XaiProviderAdapter adapter(&network);
    configure(adapter, server);
    QUrl baseEndpoint = server.baseUrl();
    baseEndpoint.setPath(QStringLiteral("//"));
    adapter.setBaseEndpoint(baseEndpoint);
    adapter.setEnvironment({{QStringLiteral("XAI_TEAM_ID"), QStringLiteral("team-1234")}});
    adapter.setCredentialOverrides(
        {{QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("management-key")}});
    QSignalSpy finished(&adapter, &XaiProviderAdapter::refreshFinished);

    adapter.refresh();
    // A UTC date rollover during the refresh must not move its history window.
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
             QStringLiteral("2027-01-15"));
    QCOMPARE(server.requests.size(), 2);
    QCOMPARE(server.requests.at(0).method, QByteArray("GET"));
    QCOMPARE(server.requests.at(0).path, QByteArray("/v1/billing/teams/team-1234/prepaid/balance"));
    QCOMPARE(server.requests.at(0).headers.value("authorization"),
             QByteArray("Bearer management-key"));
    QCOMPARE(server.requests.at(0).headers.value("accept"), QByteArray("application/json"));
    QCOMPARE(server.requests.at(1).method, QByteArray("POST"));
    QCOMPARE(server.requests.at(1).path, QByteArray("/v1/billing/teams/team-1234/usage"));
    const QJsonObject analytics = QJsonDocument::fromJson(server.requests.at(1).body)
                                      .object()
                                      .value(QStringLiteral("analyticsRequest"))
                                      .toObject();
    const QJsonObject range = analytics.value(QStringLiteral("timeRange")).toObject();
    QCOMPARE(range.value(QStringLiteral("startTime")).toString(),
             QStringLiteral("2026-12-17 00:00:00"));
    QCOMPARE(range.value(QStringLiteral("endTime")).toString(),
             QStringLiteral("2027-01-15 08:00:00"));
    QCOMPARE(range.value(QStringLiteral("timezone")).toString(), QStringLiteral("Etc/GMT"));
    QCOMPARE(analytics.value(QStringLiteral("timeUnit")).toString(),
             QStringLiteral("TIME_UNIT_DAY"));
    QCOMPARE(adapter.provider().value(QStringLiteral("id")).toString(), QStringLiteral("xai"));
    QCOMPARE(adapter.provider()
                 .value(QStringLiteral("cost"))
                 .toMap()
                 .value(QStringLiteral("balanceUSD"))
                 .toDouble(),
             10.0);
    QVERIFY(adapter.error().isEmpty());
    QVERIFY(!adapter.busy());
}

void XaiProviderAdapterTest::isolatesNamedKeyAndTeam()
{
    HttpServer server;
    for (int i = 0; i < 3; ++i) {
        server.enqueue({200, R"({"total":{"val":"-1000"}})"});
        server.enqueue({200, usagePayload()});
    }
    QNetworkAccessManager network;
    XaiProviderAdapter adapter(&network);
    configure(adapter, server);
    adapter.setCredentialOverrides({{"XAI_MANAGEMENT_API_KEY", "default-wallet"}});
    QSignalSpy finished(&adapter, &XaiProviderAdapter::refreshFinished);
    adapter.setAccountCredential(QStringLiteral("named-key"), {}, QStringLiteral("named-team"));
    adapter.refresh();
    adapter.setAccountCredential(QStringLiteral("next-key"), {}, QStringLiteral("next-team"));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(server.requests.size(), 2);
    QCOMPARE(server.requests.at(0).path,
             QByteArray("/v1/billing/teams/named-team/prepaid/balance"));
    QCOMPARE(server.requests.at(1).path, QByteArray("/v1/billing/teams/named-team/usage"));
    QCOMPARE(server.requests.at(0).headers.value("authorization"), QByteArray("Bearer named-key"));
    QCOMPARE(server.requests.at(1).headers.value("authorization"), QByteArray("Bearer named-key"));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(server.requests.at(2).path, QByteArray("/v1/billing/teams/next-team/prepaid/balance"));
    QCOMPARE(server.requests.at(3).headers.value("authorization"), QByteArray("Bearer next-key"));
    adapter.setAccountCredential(QStringLiteral("named"));
    adapter.refresh();
    QCOMPARE(finished.count(), 3);
    QCOMPARE(finished.last().first().toBool(), false);
    adapter.setAccountCredential(QString{}, {}, QStringLiteral("named-team"));
    adapter.refresh();
    QCOMPARE(finished.count(), 4);
    QCOMPARE(finished.last().first().toBool(), false);
    QCOMPARE(server.requests.size(), 4);
    adapter.setAccountCredential(std::nullopt);
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 5);
    QCOMPARE(server.requests.size(), 6);
    QCOMPARE(server.requests.at(4).path, QByteArray("/v1/billing/teams/team-1234/prepaid/balance"));
    QCOMPARE(server.requests.at(5).headers.value("authorization"),
             QByteArray("Bearer management-key"));
}

void XaiProviderAdapterTest::preservesBalanceWhenHistoryIsUnavailable()
{
    HttpServer server;
    server.enqueue({200, R"({"total":{"val":"-250"}})"});
    server.enqueue({500, "{}"});
    server.enqueue({200, R"({"total":{"val":"-250"}})"});
    server.enqueue({200, "{}"});
    QNetworkAccessManager network;
    XaiProviderAdapter adapter(&network);
    configure(adapter, server);
    QSignalSpy finished(&adapter, &XaiProviderAdapter::refreshFinished);

    adapter.refresh();
    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), true);
    QVariantMap cost = adapter.provider().value(QStringLiteral("cost")).toMap();
    QCOMPARE(cost.value(QStringLiteral("balanceUSD")).toDouble(), 2.5);
    QVERIFY(!cost.contains(QStringLiteral("last30DaysUSD")));

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).first().toBool(), true);
    cost = adapter.provider().value(QStringLiteral("cost")).toMap();
    QVERIFY(!cost.contains(QStringLiteral("last30DaysUSD")));
}

void XaiProviderAdapterTest::reportsCredentialFailures()
{
    QNetworkAccessManager network;
    XaiProviderAdapter adapter(&network);
    QSignalSpy finished(&adapter, &XaiProviderAdapter::refreshFinished);

    adapter.setEnvironment({});
    adapter.refresh();
    QCOMPARE(finished.count(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("xAI Management API key is missing"));

    adapter.setEnvironment({{QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("key")}});
    adapter.refresh();
    QCOMPARE(finished.count(), 2);
    QCOMPARE(adapter.error(), QStringLiteral("xAI team ID is missing"));

    adapter.setEnvironment({{QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("key")},
                            {QStringLiteral("XAI_TEAM_ID"), QStringLiteral("../team")}});
    adapter.refresh();
    QCOMPARE(finished.count(), 3);
    QCOMPARE(adapter.error(),
             QStringLiteral("xAI team ID must be a single identifier without path separators"));
}

void XaiProviderAdapterTest::classifiesBalanceHttpFailures()
{
    HttpServer server;
    QNetworkAccessManager network;
    XaiProviderAdapter adapter(&network);
    configure(adapter, server);
    QSignalSpy finished(&adapter, &XaiProviderAdapter::refreshFinished);

    const QList<QPair<int, QString>> failures{
        {401, QStringLiteral("xAI rejected the Management API key")},
        {403, QStringLiteral("xAI rejected the Management API key")},
        {404, QStringLiteral("xAI team was not found")},
        {429, QStringLiteral("xAI Management API rate limit exceeded")},
        {503, QStringLiteral("xAI balance request failed with HTTP 503")},
    };
    for (const auto &[status, message] : failures) {
        server.enqueue({status, "{}"});
        adapter.refresh();
        QTRY_COMPARE(finished.count(), failures.indexOf({status, message}) + 1);
        QCOMPARE(adapter.error(), message);
    }
}

void XaiProviderAdapterTest::reportsBalanceParseAndNetworkFailures()
{
    HttpServer server;
    QNetworkAccessManager network;
    XaiProviderAdapter adapter(&network);
    configure(adapter, server);
    QSignalSpy finished(&adapter, &XaiProviderAdapter::refreshFinished);

    server.enqueue({200, R"({"total":{"val":"n/a"}})"});
    adapter.refresh();
    QVERIFY(finished.wait());
    QCOMPARE(adapter.error(), QStringLiteral("xAI balance API did not return a valid cent amount"));

    adapter.setBaseEndpoint(QUrl(QStringLiteral("http://127.0.0.1:1")));
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QVERIFY(
        adapter.error().startsWith(QStringLiteral("xAI balance request failed: network error")));

    server.enqueue({200, R"({"total":{"val":"-100"}})"});
    server.enqueue({500, "{}"});
    adapter.setBaseEndpoint(server.baseUrl());
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QVERIFY(adapter.error().isEmpty());
}

void XaiProviderAdapterTest::rejectsUnauthorizedHistory()
{
    HttpServer server;
    server.enqueue({200, R"({"total":{"val":"-1000"}})"});
    server.enqueue({401, "{}"});
    server.enqueue({200, R"({"total":{"val":"-1000"}})"});
    server.enqueue({403, "{}"});
    QNetworkAccessManager network;
    XaiProviderAdapter adapter(&network);
    configure(adapter, server);
    QSignalSpy finished(&adapter, &XaiProviderAdapter::refreshFinished);

    adapter.refresh();

    QVERIFY(finished.wait());
    QCOMPARE(finished.first().first().toBool(), false);
    QCOMPARE(adapter.error(), QStringLiteral("xAI rejected the Management API key"));

    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).first().toBool(), false);
    QCOMPARE(adapter.error(), QStringLiteral("xAI rejected the Management API key"));
}

void XaiProviderAdapterTest::boundsResponsesAndTimeouts()
{
    HttpServer server;
    QNetworkAccessManager network;
    XaiProviderAdapter adapter(&network);
    configure(adapter, server);
    adapter.setTimeoutMilliseconds(20);
    QSignalSpy finished(&adapter, &XaiProviderAdapter::refreshFinished);

    server.enqueue({200, "{}", false});
    adapter.refresh();
    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);
    QCOMPARE(server.requests.size(), 1);
    QCOMPARE(adapter.error(), QStringLiteral("xAI balance request timed out"));

    adapter.setTimeoutMilliseconds(1000);
    server.enqueue({200, QByteArray(XaiProviderAdapter::MaximumResponseSize + 1, 'x')});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(adapter.error(), QStringLiteral("xAI balance response exceeds the 1 MiB limit"));

    server.enqueue({200, R"({"total":{"val":"-1000"}})"});
    server.enqueue({200, QByteArray(XaiProviderAdapter::MaximumResponseSize + 1, 'x')});
    adapter.refresh();
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(finished.at(2).first().toBool(), true);

    adapter.setTimeoutMilliseconds(20);
    server.enqueue({200, R"({"total":{"val":"-1000"}})"});
    server.enqueue({200, "{}", false});
    adapter.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 4, 1000);
    QCOMPARE(finished.at(3).first().toBool(), true);
}

QTEST_GUILESS_MAIN(XaiProviderAdapterTest)

#include "tst_xai_provider_adapter.moc"
