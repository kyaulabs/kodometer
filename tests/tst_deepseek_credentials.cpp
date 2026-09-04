#include <kodometer/deepseek_credentials.hpp>

#include <QTest>

using namespace Kodometer;

class DeepSeekCredentialsTest final : public QObject
{
    Q_OBJECT

  private slots:
    void resolvesProviderSpecificKeys();
    void prefersPrimaryKey();
    void rejectsMissingAndUnsafeKeys();
};

void DeepSeekCredentialsTest::resolvesProviderSpecificKeys()
{
    QString error;
    auto credentials = DeepSeekCredentialResolver::resolve(
        {{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("  'primary-key'  ")}}, &error);
    QVERIFY(credentials.has_value());
    QCOMPARE(credentials->apiKey, QStringLiteral("primary-key"));
    QVERIFY(error.isEmpty());

    credentials = DeepSeekCredentialResolver::resolve(
        {{QStringLiteral("DEEPSEEK_KEY"), QStringLiteral("\"legacy-key\"")}}, &error);
    QVERIFY(credentials.has_value());
    QCOMPARE(credentials->apiKey, QStringLiteral("legacy-key"));
}

void DeepSeekCredentialsTest::prefersPrimaryKey()
{
    const auto credentials = DeepSeekCredentialResolver::resolve(
        {{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("primary")},
         {QStringLiteral("DEEPSEEK_KEY"), QStringLiteral("alias")}});
    QVERIFY(credentials.has_value());
    QCOMPARE(credentials->apiKey, QStringLiteral("primary"));
}

void DeepSeekCredentialsTest::rejectsMissingAndUnsafeKeys()
{
    QString error;
    QVERIFY(!DeepSeekCredentialResolver::resolve({}, &error).has_value());
    QCOMPARE(error, QStringLiteral("DeepSeek API key is missing"));

    QVERIFY(!DeepSeekCredentialResolver::resolve(
                 {{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("  ")}}, &error)
                 .has_value());
    QCOMPARE(error, QStringLiteral("DeepSeek API key is missing"));

    QVERIFY(!DeepSeekCredentialResolver::resolve(
                 {{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("line\nbreak")}}, &error)
                 .has_value());
    QCOMPARE(error, QStringLiteral("DeepSeek API key contains invalid characters"));

    QVERIFY(!DeepSeekCredentialResolver::resolve(
                 {{QStringLiteral("DEEPSEEK_API_KEY"), QStringLiteral("line\rbreak")}}, &error)
                 .has_value());
    QCOMPARE(error, QStringLiteral("DeepSeek API key contains invalid characters"));
    QVERIFY(!DeepSeekCredentialResolver::resolve({}).has_value());
}

QTEST_GUILESS_MAIN(DeepSeekCredentialsTest)
#include "tst_deepseek_credentials.moc"
