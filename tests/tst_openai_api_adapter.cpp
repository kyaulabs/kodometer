#include <kodometer/openai_api_adapter.hpp>

#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QtTest>

using Kodometer::OpenAIApiAdapter;

class OpenAIApiAdapterTest : public QObject
{
    Q_OBJECT
  private slots:
    void init()
    {
        qunsetenv("OPENAI_ADMIN_KEY");
    }
    void missingKeyDoesNotRequest()
    {
        QNetworkAccessManager network;
        OpenAIApiAdapter adapter(&network);
        QSignalSpy success(&adapter, &OpenAIApiAdapter::refreshSucceeded);
        adapter.refresh();
        QCOMPARE(success.size(), 1);
        QVERIFY(!success.first().first().toMap().value("configured").toBool());
        adapter.setCredentialOverrides({{"OPENAI_ADMIN_KEY", "synthetic"}});
        const auto walletContext = adapter.defaultCredentialContext();
        qputenv("OPENAI_ADMIN_KEY", "environment");
        QVERIFY(adapter.defaultCredentialContext() != walletContext);
        qunsetenv("OPENAI_ADMIN_KEY");
    }
    void rejectsUnsafeCredentialsWithoutRequest()
    {
        OpenAIApiAdapter adapter;
        QSignalSpy failure(&adapter, &OpenAIApiAdapter::refreshFailed);
        for (const auto &key : {QStringLiteral("bad\nkey"), QStringLiteral("bad key"),
                                QString::fromUtf8("secret\xc3\xa9"), QString(65537, 'x')}) {
            adapter.setCredentialOverrides({{"OPENAI_ADMIN_KEY", key}});
            adapter.refresh();
        }
        QCOMPARE(failure.size(), 4);
    }

    void boundedOfficialContract_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<bool>("success");
        QTest::newRow("complete")
            << 200 << QByteArray(R"({"object":"page","data":[],"has_more":false,"next_page":null})")
            << true;
        QTest::newRow("auth") << 401 << QByteArray("private diagnostic must not escape") << false;
        QTest::newRow("forbidden") << 403 << QByteArray("synthetic-secret") << false;
        QTest::newRow("redirect") << 302 << QByteArray() << false;
        QTest::newRow("malformed") << 200 << QByteArray("{") << false;
        QTest::newRow("partial")
            << 200
            << QByteArray(R"({"object":"page","data":[],"has_more":true,"next_page":"cursor"})")
            << false;
        QTest::newRow("oversize") << 200 << QByteArray(1024 * 1024 + 1, 'x') << false;
    }
    void boundedOfficialContract()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(bool, success);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray request;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, &server, [&, socket] {
                request += socket->readAll();
                if (!request.contains("\r\n\r\n"))
                    return;
                socket->write("HTTP/1.1 " + QByteArray::number(status) +
                              " Result\r\nContent-Length: " + QByteArray::number(body.size()) +
                              "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        OpenAIApiAdapter adapter;
        adapter.setCredentialOverrides({{"OPENAI_ADMIN_KEY", "synthetic-admin"}});
        adapter.setEndpoint(
            QUrl(QStringLiteral("http://127.0.0.1:%1/costs").arg(server.serverPort())));
        adapter.setUsageEndpoint(
            QUrl(QStringLiteral("http://127.0.0.1:%1/usage").arg(server.serverPort())));
        QSignalSpy ok(&adapter, &OpenAIApiAdapter::refreshSucceeded);
        QSignalSpy fail(&adapter, &OpenAIApiAdapter::refreshFailed);
        adapter.refresh();
        adapter.refresh(); // No overlapping request.
        QTRY_COMPARE(ok.size() + fail.size(), 1);
        QCOMPARE(ok.size(), success ? 1 : 0);
        QVERIFY(request.contains("Authorization: Bearer synthetic-admin"));
        QVERIFY(request.contains("bucket_width=1d"));
        QVERIFY(request.contains("limit=62"));
        QVERIFY(!request.contains("group_by"));
        if (!success)
            QVERIFY(!fail.first().first().toString().contains("synthetic"));
    }
    void optionalUsagePreservesCosts_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("usage");
        QTest::addColumn<bool>("available");
        QTest::newRow("cached") << 200 << QByteArray("valid") << true;
        QTest::newRow("forbidden") << 403 << QByteArray("private") << false;
        QTest::newRow("redirect") << 302 << QByteArray() << false;
        QTest::newRow("malformed") << 200 << QByteArray("{") << false;
        QTest::newRow("oversize") << 200 << QByteArray(1024 * 1024 + 1, 'x') << false;
        QTest::newRow("timeout") << 0 << QByteArray() << false;
    }
    void optionalUsagePreservesCosts()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, usage);
        QFETCH(bool, available);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        OpenAIApiAdapter adapter;
        adapter.setCredentialOverrides({{"OPENAI_ADMIN_KEY", "original-synthetic"}});
        const QUrl base(QStringLiteral("http://127.0.0.1:%1/").arg(server.serverPort()));
        adapter.setEndpoint(base.resolved(QUrl("costs")));
        adapter.setUsageEndpoint(base.resolved(QUrl("usage")));
        adapter.setTimeoutMilliseconds(1000);
        QList<QByteArray> requests;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            auto request = QSharedPointer<QByteArray>::create();
            connect(socket, &QTcpSocket::readyRead, &server, [&, socket, request] {
                *request += socket->readAll();
                if (!request->contains("\r\n\r\n"))
                    return;
                requests.append(*request);
                const bool costs = request->startsWith("GET /costs?");
                QByteArray body =
                    R"({"object":"page","data":[],"has_more":false,"next_page":null})";
                if (!costs) {
                    if (status == 0)
                        return;
                    if (available) {
                        const QUrl url(QString::fromUtf8(request->split(' ').at(1)));
                        const qint64 start =
                            QUrlQuery(url).queryItemValue("start_time").toLongLong();
                        body =
                            "{\"object\":\"page\",\"has_more\":false,\"next_page\":null,\"data\":["
                            "{\"object\":\"bucket\",\"start_time\":" +
                            QByteArray::number(start) +
                            ",\"end_time\":" + QByteArray::number(start + 86400) +
                            ",\"results\":[{\"object\":\"organization.usage.completions.result\","
                            "\"input_tokens\":100,\"input_cached_tokens\":25}]}]}";
                    }
                    else {
                        body = usage;
                    }
                }
                adapter.setCredentialOverrides({{"OPENAI_ADMIN_KEY", "replacement-synthetic"}});
                adapter.refresh(); // Neither stage may overlap; the originating key stays pinned.
                socket->write("HTTP/1.1 " + QByteArray::number(costs ? 200 : status) +
                              " Result\r\nContent-Length: " + QByteArray::number(body.size()) +
                              "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        QSignalSpy ok(&adapter, &OpenAIApiAdapter::refreshSucceeded);
        QSignalSpy fail(&adapter, &OpenAIApiAdapter::refreshFailed);
        adapter.refresh();
        QTRY_COMPARE(ok.size(), 1);
        QVERIFY(fail.isEmpty());
        QCOMPARE(requests.size(), 2);
        for (const auto &request : requests)
            QVERIFY(request.contains("Authorization: Bearer original-synthetic"));
        QVERIFY(requests.last().contains("limit=31"));
        QVERIFY(!requests.last().contains("group_by"));
        const auto result = ok.first().first().toMap();
        QVERIFY(result.value("configured").toBool());
        QVERIFY(result.contains("cost"));
        QCOMPARE(result.value("usage").toMap().contains("cachedInputPercent"), available);
        if (available) {
            QCOMPARE(result.value("usage").toMap().value("cachedInputPercent").toDouble(), 25.0);
            available = false;
            usage = "{";
            adapter.refresh();
            QTRY_COMPARE(ok.size(), 2);
            QVERIFY(!ok.last().first().toMap().contains("usage"));
            QCOMPARE(requests.size(), 4);
            QVERIFY(requests.last().contains("Authorization: Bearer replacement-synthetic"));
        }
    }

    void timeoutAndNetworkFailure()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        OpenAIApiAdapter adapter;
        adapter.setCredentialOverrides({{"OPENAI_ADMIN_KEY", "synthetic"}});
        adapter.setEndpoint(QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(server.serverPort())));
        adapter.setTimeoutMilliseconds(1);
        QSignalSpy fail(&adapter, &OpenAIApiAdapter::refreshFailed);
        adapter.refresh();
        QTRY_COMPARE(fail.size(), 1);
        QVERIFY(fail.first().first().toString().contains("timed out"));
        server.close();
        adapter.setTimeoutMilliseconds(1000);
        adapter.refresh();
        QTRY_COMPARE(fail.size(), 2);
    }
};
QTEST_GUILESS_MAIN(OpenAIApiAdapterTest)
#include "tst_openai_api_adapter.moc"
