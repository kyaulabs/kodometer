#include <kodometer/xai_credentials.hpp>

#include <QtTest>

using Kodometer::XaiCredentialResolver;

class XaiCredentialsTest final : public QObject
{
    Q_OBJECT

  private slots:
    void resolvesAndCleansEnvironmentCredentials();
    void rejectsMissingAndInvalidCredentials();
};

void XaiCredentialsTest::resolvesAndCleansEnvironmentCredentials()
{
    QString error;
    const auto credentials = XaiCredentialResolver::resolve(
        {{QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("  'management-key'  ")},
         {QStringLiteral("XAI_TEAM_ID"), QStringLiteral(" \"team-1234\" ")}},
        &error);

    QVERIFY2(credentials.has_value(), qPrintable(error));
    QCOMPARE(credentials->managementApiKey, QStringLiteral("management-key"));
    QCOMPARE(credentials->teamId, QStringLiteral("team-1234"));

    const auto unmatchedQuotes = XaiCredentialResolver::resolve(
        {{QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("'key")},
         {QStringLiteral("XAI_TEAM_ID"), QStringLiteral("team")}},
        &error);
    QVERIFY(unmatchedQuotes.has_value());
    QCOMPARE(unmatchedQuotes->managementApiKey, QStringLiteral("'key"));
}

void XaiCredentialsTest::rejectsMissingAndInvalidCredentials()
{
    QString error;
    QVERIFY(!XaiCredentialResolver::resolve({}, &error));
    QCOMPARE(error, QStringLiteral("xAI Management API key is missing"));

    QVERIFY(!XaiCredentialResolver::resolve(
        {{QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("  ")},
         {QStringLiteral("XAI_TEAM_ID"), QStringLiteral("team")}},
        &error));
    QCOMPARE(error, QStringLiteral("xAI Management API key is missing"));

    QVERIFY(!XaiCredentialResolver::resolve(
        {{QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("key")}}, &error));
    QCOMPARE(error, QStringLiteral("xAI team ID is missing"));

    for (const QString &team : {QStringLiteral("."), QStringLiteral(".."),
                                QStringLiteral("team/other")}) {
        QVERIFY(!XaiCredentialResolver::resolve(
            {{QStringLiteral("XAI_MANAGEMENT_API_KEY"), QStringLiteral("key")},
             {QStringLiteral("XAI_TEAM_ID"), team}},
            &error));
        QCOMPARE(error,
                 QStringLiteral("xAI team ID must be a single identifier without path separators"));
    }
    QVERIFY(!XaiCredentialResolver::resolve({}, nullptr));
}

QTEST_GUILESS_MAIN(XaiCredentialsTest)

#include "tst_xai_credentials.moc"
