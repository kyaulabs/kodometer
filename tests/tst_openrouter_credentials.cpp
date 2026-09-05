#include <kodometer/openrouter_credentials.hpp>

#include <QtTest>

using Kodometer::OpenRouterCredentialResolver;

class OpenRouterCredentialTest final : public QObject
{
    Q_OBJECT

  private slots:
    void resolvesCredentialsAndHeaders();
    void reportsInvalidCredentials_data();
    void reportsInvalidCredentials();
};

void OpenRouterCredentialTest::resolvesCredentialsAndHeaders()
{
    QString error;
    const auto credentials = OpenRouterCredentialResolver::resolve(
        {{QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("  'sk-or-v1-user'  ")},
         {QStringLiteral("OPENROUTER_MANAGEMENT_API_KEY"), QStringLiteral("management-key")},
         {QStringLiteral("OPENROUTER_HTTP_REFERER"), QStringLiteral(" https://example.test/app ")},
         {QStringLiteral("OPENROUTER_X_TITLE"), QStringLiteral("Kodometer test")}},
        &error);

    QVERIFY(credentials.has_value());
    QCOMPARE(credentials->apiKey, QStringLiteral("sk-or-v1-user"));
    QCOMPARE(credentials->managementApiKey, QStringLiteral("management-key"));
    QCOMPARE(credentials->httpReferer, QStringLiteral("https://example.test/app"));
    QCOMPARE(credentials->clientTitle, QStringLiteral("Kodometer test"));
    QVERIFY(error.isEmpty());

    const auto defaults = OpenRouterCredentialResolver::resolve(
        {{QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("key")}});
    QVERIFY(defaults.has_value());
    QVERIFY(defaults->managementApiKey.isEmpty());
    QVERIFY(defaults->httpReferer.isEmpty());
    QCOMPARE(defaults->clientTitle, QStringLiteral("Kodometer"));
}

void OpenRouterCredentialTest::reportsInvalidCredentials_data()
{
    QTest::addColumn<QMap<QString, QString>>("environment");
    QTest::addColumn<QString>("message");

    QTest::newRow("missing") << QMap<QString, QString>{}
                              << QStringLiteral("OpenRouter API key is missing");
    QTest::newRow("api key newline")
        << QMap<QString, QString>{{QStringLiteral("OPENROUTER_API_KEY"),
                                  QStringLiteral("key\nsecond")}}
        << QStringLiteral("OpenRouter API key contains invalid characters");
    QTest::newRow("management newline")
        << QMap<QString, QString>{{QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("key")},
                                  {QStringLiteral("OPENROUTER_MANAGEMENT_API_KEY"),
                                   QStringLiteral("management\rkey")}}
        << QStringLiteral("OpenRouter Management API key contains invalid characters");
    QTest::newRow("referer newline")
        << QMap<QString, QString>{{QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("key")},
                                  {QStringLiteral("OPENROUTER_HTTP_REFERER"),
                                   QStringLiteral("https://example.test\nInjected: yes")}}
        << QStringLiteral("OpenRouter HTTP referer contains invalid characters");
    QTest::newRow("title newline")
        << QMap<QString, QString>{{QStringLiteral("OPENROUTER_API_KEY"), QStringLiteral("key")},
                                  {QStringLiteral("OPENROUTER_X_TITLE"),
                                   QStringLiteral("title\nInjected: yes")}}
        << QStringLiteral("OpenRouter client title contains invalid characters");
}

void OpenRouterCredentialTest::reportsInvalidCredentials()
{
    QFETCH(QMap<QString, QString>, environment);
    QFETCH(QString, message);

    QString error;
    QVERIFY(!OpenRouterCredentialResolver::resolve(environment, &error).has_value());
    QCOMPARE(error, message);
}

QTEST_GUILESS_MAIN(OpenRouterCredentialTest)

#include "tst_openrouter_credentials.moc"
