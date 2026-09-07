#include <kodometer/gemini_credentials.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

using Kodometer::GeminiAuthType;
using Kodometer::GeminiCredentials;
using Kodometer::GeminiCredentialStore;
using Kodometer::GeminiOAuthConfig;

namespace {

bool writePrivateFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(data) != data.size()) {
        return false;
    }
    file.close();
    return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QString tokenFor(const QJsonObject &claims)
{
    const QByteArray payload =
        QJsonDocument(claims)
            .toJson(QJsonDocument::Compact)
            .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QStringLiteral("header.%1.signature").arg(QString::fromLatin1(payload));
}

QByteArray credentialDocument(qint64 expiresAt = 1'800'000'000'000)
{
    return QJsonDocument(
               QJsonObject{{QStringLiteral("access_token"), QStringLiteral("access-token")},
                           {QStringLiteral("refresh_token"), QStringLiteral("refresh-token")},
                           {QStringLiteral("id_token"),
                            tokenFor({{QStringLiteral("email"), QStringLiteral("user@example.com")},
                                      {QStringLiteral("hd"), QStringLiteral("example.com")}})},
                           {QStringLiteral("expiry_date"), expiresAt},
                           {QStringLiteral("token_type"), QStringLiteral("Bearer")},
                           {QStringLiteral("preserved"), 7}})
        .toJson();
}

} // namespace

class GeminiCredentialsTest final : public QObject
{
    Q_OBJECT

  private slots:
    void resolvesPathsAndAuthentication();
    void parsesOAuthCredentialsAndClaims();
    void rejectsInvalidDocuments();
    void enforcesPrivateFiles();
    void savesRotatedCredentialsAtomically();
    void resolvesOAuthClientConfiguration();
    void calculatesRefreshNeed();
};

void GeminiCredentialsTest::resolvesPathsAndAuthentication()
{
    QCOMPARE(GeminiCredentialStore::authenticationFilePath(QStringLiteral("/home/test")),
             QStringLiteral("/home/test/.gemini/oauth_creds.json"));
    QCOMPARE(GeminiCredentialStore::settingsFilePath(QStringLiteral("/home/test")),
             QStringLiteral("/home/test/.gemini/settings.json"));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settings = directory.filePath(QStringLiteral("settings.json"));
    QCOMPARE(GeminiCredentialStore::selectedAuthentication(settings), GeminiAuthType::Unknown);
    QVERIFY(writePrivateFile(settings, "{"));
    QCOMPARE(GeminiCredentialStore::selectedAuthentication(settings), GeminiAuthType::Unknown);
    QVERIFY(
        writePrivateFile(settings, R"({"security":{"auth":{"selectedType":"oauth-personal"}}})"));
    QCOMPARE(GeminiCredentialStore::selectedAuthentication(settings),
             GeminiAuthType::OAuthPersonal);
    QVERIFY(writePrivateFile(settings, R"({"security":{"auth":{"selectedType":"api-key"}}})"));
    QCOMPARE(GeminiCredentialStore::selectedAuthentication(settings), GeminiAuthType::ApiKey);
    QVERIFY(
        writePrivateFile(settings, R"({"security":{"auth":{"selectedType":"gemini-api-key"}}})"));
    QCOMPARE(GeminiCredentialStore::selectedAuthentication(settings), GeminiAuthType::ApiKey);
    QVERIFY(writePrivateFile(settings, R"({"security":{"auth":{"selectedType":"vertex-ai"}}})"));
    QCOMPARE(GeminiCredentialStore::selectedAuthentication(settings), GeminiAuthType::VertexAi);
    QVERIFY(writePrivateFile(settings, R"({"security":{"auth":{"selectedType":"future"}}})"));
    QCOMPARE(GeminiCredentialStore::selectedAuthentication(settings), GeminiAuthType::Unknown);
    QVERIFY(
        writePrivateFile(settings, QByteArray(GeminiCredentialStore::MaximumFileSize + 1, 'x')));
    QCOMPARE(GeminiCredentialStore::selectedAuthentication(settings), GeminiAuthType::Unknown);
}

void GeminiCredentialsTest::parsesOAuthCredentialsAndClaims()
{
    QString error;
    const auto credentials = GeminiCredentialStore::parse(credentialDocument(), &error);

    QVERIFY2(credentials.has_value(), qPrintable(error));
    QCOMPARE(credentials->accessToken, QStringLiteral("access-token"));
    QCOMPARE(credentials->refreshToken, QStringLiteral("refresh-token"));
    QCOMPARE(credentials->email, QStringLiteral("user@example.com"));
    QCOMPARE(credentials->hostedDomain, QStringLiteral("example.com"));
    QCOMPARE(credentials->redactedEmail(), QStringLiteral("redacted@example.com"));
    QCOMPARE(credentials->expiresAt,
             QDateTime::fromMSecsSinceEpoch(1'800'000'000'000, QTimeZone::UTC));

    const auto refreshOnly = GeminiCredentialStore::parse(
        R"({"access_token":" ","refresh_token":" refresh ","id_token":"broken","expiry_date":null})",
        &error);
    QVERIFY2(refreshOnly.has_value(), qPrintable(error));
    QVERIFY(refreshOnly->accessToken.isEmpty());
    QCOMPARE(refreshOnly->refreshToken, QStringLiteral("refresh"));
    QVERIFY(refreshOnly->email.isEmpty());
    QVERIFY(refreshOnly->redactedEmail().isEmpty());

    const QString arrayToken =
        QStringLiteral("header.%1.signature")
            .arg(QString::fromLatin1(QByteArray("[]").toBase64(QByteArray::Base64UrlEncoding |
                                                               QByteArray::OmitTrailingEquals)));
    const auto arrayClaims = GeminiCredentialStore::parse(
        QStringLiteral(R"({"access_token":"token","id_token":"%1"})").arg(arrayToken).toUtf8(),
        &error);
    QVERIFY(arrayClaims.has_value());
    QVERIFY(arrayClaims->email.isEmpty());

    GeminiCredentials redacted;
    redacted.email = QStringLiteral("local-name");
    QCOMPARE(redacted.redactedEmail(), QStringLiteral("redacted"));
}

void GeminiCredentialsTest::rejectsInvalidDocuments()
{
    QString error;
    QVERIFY(!GeminiCredentialStore::parse("{", &error));
    QCOMPARE(error, QStringLiteral("Gemini credentials contain invalid JSON"));
    QVERIFY(!GeminiCredentialStore::parse("[]", &error));
    QCOMPARE(error, QStringLiteral("Gemini credentials must contain a JSON object"));
    QVERIFY(!GeminiCredentialStore::parse(R"({"expiry_date":1})", &error));
    QCOMPARE(error, QStringLiteral("Gemini OAuth tokens are missing"));
    QVERIFY(
        !GeminiCredentialStore::parse(R"({"access_token":"token","expiry_date":"later"})", &error));
    QCOMPARE(error, QStringLiteral("Gemini OAuth expiry is invalid"));
    QVERIFY(!GeminiCredentialStore::parse(R"({"access_token":"token","expiry_date":-1})", &error));
    QCOMPARE(error, QStringLiteral("Gemini OAuth expiry is invalid"));
    QVERIFY(!GeminiCredentialStore::parse("{", nullptr));
}

void GeminiCredentialsTest::enforcesPrivateFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString error;
    const QString path = directory.filePath(QStringLiteral("oauth_creds.json"));

    QVERIFY(!GeminiCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Gemini credentials were not found"));
    QVERIFY(!GeminiCredentialStore::load(directory.path(), &error));
    QCOMPARE(error, QStringLiteral("Gemini credential path is not a regular file"));
    QVERIFY(writePrivateFile(path, credentialDocument()));
    QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                            QFileDevice::ReadGroup));
    QVERIFY(!GeminiCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Gemini credential permissions expose secrets"));

    QVERIFY(QFile::remove(path));
    QVERIFY(QFile::link(directory.filePath(QStringLiteral("missing")), path));
    QVERIFY(!GeminiCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Gemini credentials must not be a symbolic link"));

    QVERIFY(QFile::remove(path));
    QVERIFY(writePrivateFile(path, QByteArray(GeminiCredentialStore::MaximumFileSize + 1, 'x')));
    QVERIFY(!GeminiCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Gemini credentials exceed the 1 MiB limit"));
}

void GeminiCredentialsTest::savesRotatedCredentialsAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("oauth_creds.json"));
    QVERIFY(writePrivateFile(path, credentialDocument()));
    QString error;
    auto credentials = GeminiCredentialStore::load(path, &error);
    QVERIFY2(credentials.has_value(), qPrintable(error));

    credentials->accessToken = QStringLiteral("new-access");
    credentials->refreshToken = QStringLiteral("new-refresh");
    credentials->idToken = QStringLiteral("new-id");
    credentials->expiresAt = QDateTime::fromMSecsSinceEpoch(1'900'000'000'000, QTimeZone::UTC);
    QVERIFY2(GeminiCredentialStore::save(path, *credentials, &error), qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    QCOMPARE(root.value(QStringLiteral("access_token")).toString(), QStringLiteral("new-access"));
    QCOMPARE(root.value(QStringLiteral("refresh_token")).toString(), QStringLiteral("new-refresh"));
    QCOMPARE(root.value(QStringLiteral("id_token")).toString(), QStringLiteral("new-id"));
    QCOMPARE(root.value(QStringLiteral("expiry_date")).toVariant().toLongLong(), 1'900'000'000'000);
    QCOMPARE(root.value(QStringLiteral("preserved")).toInt(), 7);
    QCOMPARE(file.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                                   QFileDevice::ReadOther | QFileDevice::WriteOther),
             QFileDevice::Permissions{});
    file.close();

    credentials->refreshToken.clear();
    credentials->idToken.clear();
    credentials->expiresAt = {};
    QVERIFY2(GeminiCredentialStore::save(path, *credentials, &error), qPrintable(error));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject cleared = QJsonDocument::fromJson(file.readAll()).object();
    QVERIFY(!cleared.contains(QStringLiteral("refresh_token")));
    QVERIFY(!cleared.contains(QStringLiteral("id_token")));
    QVERIFY(!cleared.contains(QStringLiteral("expiry_date")));
    file.close();

    QVERIFY(!GeminiCredentialStore::save(directory.filePath(QStringLiteral("missing")),
                                         *credentials, &error));
    QCOMPARE(error, QStringLiteral("Gemini credentials were not found"));
}

void GeminiCredentialsTest::resolvesOAuthClientConfiguration()
{
    const auto environmentClient = GeminiOAuthConfig::resolve(
        {{QStringLiteral("GEMINI_OAUTH_CLIENT_ID"), QStringLiteral(" client-id ")},
         {QStringLiteral("GEMINI_OAUTH_CLIENT_SECRET"), QStringLiteral(" client-secret ")}});
    QCOMPARE(environmentClient.clientId, QStringLiteral("client-id"));
    QCOMPARE(environmentClient.clientSecret, QStringLiteral("client-secret"));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString script = directory.filePath(QStringLiteral("oauth2.js"));
    QVERIFY(writePrivateFile(script, "const OAUTH_CLIENT_ID = 'js-id.apps.googleusercontent.com';\n"
                                     "const OAUTH_CLIENT_SECRET = \"js-secret\";\n"));
    const auto scriptClient = GeminiOAuthConfig::resolve(
        {{QStringLiteral("GEMINI_OAUTH_CLIENT_ID"), QStringLiteral("incomplete")},
         {QStringLiteral("GEMINI_OAUTH2_JS_PATH"), script}});
    QCOMPARE(scriptClient.clientId, QStringLiteral("js-id.apps.googleusercontent.com"));
    QCOMPARE(scriptClient.clientSecret, QStringLiteral("js-secret"));

    QVERIFY(writePrivateFile(script, "const unrelated = true;"));
    const QMap<QString, QString> isolatedEnvironment{
        {QStringLiteral("GEMINI_OAUTH2_JS_PATH"), script},
        {QStringLiteral("PATH"), directory.filePath(QStringLiteral("empty-bin"))},
        {QStringLiteral("HOME"), directory.path()},
    };
    const auto invalidScript = GeminiOAuthConfig::resolve(isolatedEnvironment);
    QVERIFY(invalidScript.clientId.isEmpty());
    QVERIFY(writePrivateFile(script, QByteArray(GeminiCredentialStore::MaximumFileSize + 1, 'x')));
    const auto oversizedScript = GeminiOAuthConfig::resolve(isolatedEnvironment);
    QVERIFY(oversizedScript.clientId.isEmpty());

    const QString cliScript =
        directory.filePath(QStringLiteral("lib/node_modules/@google/gemini-cli/dist/index.js"));
    const QString coreScript = directory.filePath(
        QStringLiteral("lib/node_modules/@google/gemini-cli-core/dist/src/code_assist/oauth2.js"));
    const QString executable = directory.filePath(QStringLiteral("bin/gemini"));
    QVERIFY(QDir().mkpath(QFileInfo(cliScript).absolutePath()));
    QVERIFY(QDir().mkpath(QFileInfo(coreScript).absolutePath()));
    QVERIFY(QDir().mkpath(QFileInfo(executable).absolutePath()));
    QVERIFY(writePrivateFile(cliScript, "#!/usr/bin/env node\n"));
    QVERIFY(QFile::setPermissions(cliScript, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                 QFileDevice::ExeOwner));
    QVERIFY(QFile::link(cliScript, executable));
    QVERIFY(writePrivateFile(coreScript, "const OAUTH_CLIENT_ID = 'discovered-id';\n"
                                         "const OAUTH_CLIENT_SECRET = 'discovered-secret';\n"));
    const auto discovered =
        GeminiOAuthConfig::resolve({{QStringLiteral("PATH"), QFileInfo(executable).absolutePath()},
                                    {QStringLiteral("HOME"), directory.path()}});
    QCOMPARE(discovered.clientId, QStringLiteral("discovered-id"));
    QCOMPARE(discovered.clientSecret, QStringLiteral("discovered-secret"));
}

void GeminiCredentialsTest::calculatesRefreshNeed()
{
    const QDateTime now = QDateTime::fromSecsSinceEpoch(1'700'000'000, QTimeZone::UTC);
    GeminiCredentials credentials;

    QVERIFY(credentials.needsRefresh(now));
    credentials.accessToken = QStringLiteral("token");
    credentials.expiresAt = now.addSecs(301);
    QVERIFY(!credentials.needsRefresh(now));
    credentials.expiresAt = now.addSecs(300);
    QVERIFY(credentials.needsRefresh(now));
    credentials.accessToken.clear();
    credentials.expiresAt = now.addSecs(3600);
    QVERIFY(credentials.needsRefresh(now));
}

QTEST_GUILESS_MAIN(GeminiCredentialsTest)

#include "tst_gemini_credentials.moc"
