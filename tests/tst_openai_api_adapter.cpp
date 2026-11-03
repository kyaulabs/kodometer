#include <kodometer/openai_api_adapter.hpp>

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
        OpenAIApiAdapter adapter;
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
