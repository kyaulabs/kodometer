#include <kodometer/codex_credentials.hpp>

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

using Kodometer::CodexCredentials;
using Kodometer::CodexCredentialStore;

namespace {

QString jwt(const QJsonObject &claims)
{
    const auto encode = [](const QByteArray &value) {
        return QString::fromLatin1(
            value.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    };
    return encode("{\"alg\":\"none\"}") + QLatin1Char('.') +
           encode(QJsonDocument(claims).toJson(QJsonDocument::Compact)) +
           QStringLiteral(".signature");
}

bool writePrivateFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(contents) != contents.size()) {
        return false;
    }
    file.close();
    return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

} // namespace

class CodexCredentialsTest final : public QObject
{
    Q_OBJECT

  private slots:
    void parsesOAuthCredentials();
    void parsesCamelCaseCredentials();
    void parsesApiKey();
    void parsesJwtFallbacks();
    void rejectsInvalidDocuments();
    void resolvesCredentialPaths();
    void loadsOnlyPrivateRegularFiles();
    void rejectsOversizedFile();
    void rejectsUnsafePaths();
    void savesRotatedCredentialsAtomically();
    void rejectsInvalidRotation();
    void calculatesRefreshNeed();
    void redactsIdentity();
};

void CodexCredentialsTest::parsesOAuthCredentials()
{
    const QString idToken = jwt({
        {QStringLiteral("email"), QStringLiteral("person@example.com")},
        {QStringLiteral("chatgpt_account_id"), QStringLiteral("jwt-account")},
    });
    const QString accessToken = jwt({
        {QStringLiteral("exp"), 1'800'000'000},
        {QStringLiteral("chatgpt_account_id"), QStringLiteral("access-account")},
    });
    const QJsonObject document{
        {QStringLiteral("tokens"),
         QJsonObject{
             {QStringLiteral("access_token"), accessToken},
             {QStringLiteral("refresh_token"), QStringLiteral("refresh-token")},
             {QStringLiteral("id_token"), idToken},
             {QStringLiteral("account_id"), QStringLiteral("explicit-account")},
         }},
        {QStringLiteral("last_refresh"), QStringLiteral("2026-09-03T12:30:00Z")},
        {QStringLiteral("unknown"), 42},
    };

    QString error;
    const auto credentials = CodexCredentialStore::parse(QJsonDocument(document).toJson(), &error);

    QVERIFY2(credentials.has_value(), qPrintable(error));
    QCOMPARE(credentials->accessToken, accessToken);
    QCOMPARE(credentials->refreshToken, QStringLiteral("refresh-token"));
    QCOMPARE(credentials->idToken, idToken);
    QCOMPARE(credentials->accountId, QStringLiteral("explicit-account"));
    QCOMPARE(credentials->email, QStringLiteral("person@example.com"));
    QCOMPARE(credentials->expiresAt, QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC));
    QCOMPARE(credentials->lastRefresh,
             QDateTime::fromString(QStringLiteral("2026-09-03T12:30:00Z"), Qt::ISODate));
    QCOMPARE(credentials->document.value(QStringLiteral("unknown")).toInt(), 42);
    QVERIFY(!credentials->apiKey);
}

void CodexCredentialsTest::parsesCamelCaseCredentials()
{
    const QString accessToken = jwt({
        {QStringLiteral("https://api.openai.com/auth"),
         QJsonObject{{QStringLiteral("chatgpt_account_id"), QStringLiteral("nested-account")}}},
    });
    const QJsonObject document{
        {QStringLiteral("tokens"),
         QJsonObject{
             {QStringLiteral("accessToken"), accessToken},
             {QStringLiteral("refreshToken"), QStringLiteral("rotator")},
         }},
    };

    QString error;
    const auto credentials = CodexCredentialStore::parse(QJsonDocument(document).toJson(), &error);

    QVERIFY2(credentials.has_value(), qPrintable(error));
    QCOMPARE(credentials->accountId, QStringLiteral("nested-account"));
    QVERIFY(credentials->idToken.isEmpty());
    QVERIFY(!credentials->expiresAt.isValid());
}

void CodexCredentialsTest::parsesApiKey()
{
    const QByteArray document = R"({"OPENAI_API_KEY":"  sk-example  ","unknown":true})";
    QString error;

    const auto credentials = CodexCredentialStore::parse(document, &error);

    QVERIFY2(credentials.has_value(), qPrintable(error));
    QCOMPARE(credentials->accessToken, QStringLiteral("sk-example"));
    QVERIFY(credentials->refreshToken.isEmpty());
    QVERIFY(credentials->apiKey);
}

void CodexCredentialsTest::parsesJwtFallbacks()
{
    const QString directToken = jwt({
        {QStringLiteral("chatgpt_account_id"), QStringLiteral("direct-account")},
        {QStringLiteral("email"), QStringLiteral("access@example.com")},
    });
    QString error;
    auto credentials = CodexCredentialStore::parse(
        QJsonDocument(
            QJsonObject{
                {QStringLiteral("tokens"),
                 QJsonObject{{QStringLiteral("access_token"), directToken},
                             {QStringLiteral("refresh_token"), QStringLiteral("refresh")}}},
            })
            .toJson(),
        &error);
    QVERIFY2(credentials.has_value(), qPrintable(error));
    QCOMPARE(credentials->accountId, QStringLiteral("direct-account"));
    QCOMPARE(credentials->email, QStringLiteral("access@example.com"));

    const QString organizationToken = jwt({
        {QStringLiteral("organizations"),
         QJsonArray{QJsonObject{},
                    QJsonObject{{QStringLiteral("id"), QStringLiteral("organization-account")}}}},
        {QStringLiteral("exp"), 12.5},
    });
    credentials = CodexCredentialStore::parse(
        QJsonDocument(
            QJsonObject{
                {QStringLiteral("tokens"),
                 QJsonObject{{QStringLiteral("access_token"), organizationToken},
                             {QStringLiteral("refresh_token"), QStringLiteral("refresh")},
                             {QStringLiteral("id_token"), QStringLiteral("x.eA.signature")}}},
            })
            .toJson(),
        &error);
    QVERIFY2(credentials.has_value(), qPrintable(error));
    QCOMPARE(credentials->accountId, QStringLiteral("organization-account"));
    QVERIFY(!credentials->expiresAt.isValid());

    const QString distantToken = jwt({{QStringLiteral("exp"), 9'000'000'000'000.0}});
    credentials = CodexCredentialStore::parse(
        QJsonDocument(
            QJsonObject{
                {QStringLiteral("tokens"),
                 QJsonObject{{QStringLiteral("access_token"), distantToken},
                             {QStringLiteral("refresh_token"), QStringLiteral("refresh")}}},
            })
            .toJson(),
        &error);
    QVERIFY2(credentials.has_value(), qPrintable(error));
    QVERIFY(!credentials->expiresAt.isValid());
}

void CodexCredentialsTest::rejectsInvalidDocuments()
{
    QString error;
    QVERIFY(!CodexCredentialStore::parse({}, &error));
    QCOMPARE(error, QStringLiteral("Codex auth file contains invalid JSON"));

    QVERIFY(!CodexCredentialStore::parse("[]", &error));
    QCOMPARE(error, QStringLiteral("Codex auth file must contain a JSON object"));

    QVERIFY(!CodexCredentialStore::parse(R"({"tokens":{"access_token":"only"}})", &error));
    QCOMPARE(error, QStringLiteral("Codex auth file contains no usable credentials"));

    QVERIFY(!CodexCredentialStore::parse("{", nullptr));
}

void CodexCredentialsTest::resolvesCredentialPaths()
{
    QCOMPARE(CodexCredentialStore::authenticationFilePath(
                 {{QStringLiteral("CODEX_HOME"), QStringLiteral(" /tmp/custom-codex ")}},
                 QStringLiteral("/home/tester")),
             QStringLiteral("/tmp/custom-codex/auth.json"));
    QCOMPARE(CodexCredentialStore::authenticationFilePath({}, QStringLiteral("/home/tester")),
             QStringLiteral("/home/tester/.codex/auth.json"));
}

void CodexCredentialsTest::loadsOnlyPrivateRegularFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("auth.json"));
    QVERIFY(writePrivateFile(path,
                             R"({"tokens":{"access_token":"access","refresh_token":"refresh"}})"));

    QString error;
    QVERIFY(CodexCredentialStore::load(path, &error));

    QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                            QFileDevice::ReadGroup));
    QVERIFY(!CodexCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Codex auth file permissions expose credentials"));

    QVERIFY(QFile::remove(path));
    QVERIFY(!CodexCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Codex auth file was not found"));

    QVERIFY(QFile::link(directory.filePath(QStringLiteral("missing")), path));
    QVERIFY(!CodexCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Codex auth file must not be a symbolic link"));
}

void CodexCredentialsTest::rejectsOversizedFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("auth.json"));
    QVERIFY(writePrivateFile(path, QByteArray(1024 * 1024 + 1, 'x')));

    QString error;
    QVERIFY(!CodexCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Codex auth file exceeds the 1 MiB limit"));
}

void CodexCredentialsTest::rejectsUnsafePaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString error;

    QVERIFY(!CodexCredentialStore::load(directory.path(), &error));
    QCOMPARE(error, QStringLiteral("Codex auth path is not a regular file"));

#if defined(Q_OS_UNIX)
    if (QFileInfo(QStringLiteral("/etc/passwd")).ownerId() !=
        QFileInfo(directory.path()).ownerId()) {
        QVERIFY(!CodexCredentialStore::load(QStringLiteral("/etc/passwd"), &error));
        QCOMPARE(error, QStringLiteral("Codex auth file is owned by another user"));
    }
#endif
}

void CodexCredentialsTest::savesRotatedCredentialsAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("auth.json"));
    QVERIFY(writePrivateFile(
        path,
        R"({"tokens":{"access_token":"old","refresh_token":"old-refresh","extra":"keep"},"unknown":7})"));

    QString error;
    auto credentials = CodexCredentialStore::load(path, &error);
    QVERIFY2(credentials.has_value(), qPrintable(error));
    credentials->accessToken = QStringLiteral("new-access");
    credentials->refreshToken = QStringLiteral("new-refresh");
    credentials->idToken = QStringLiteral("new-id");
    credentials->accountId = QStringLiteral("account");
    credentials->lastRefresh =
        QDateTime::fromString(QStringLiteral("2026-09-03T15:00:00Z"), Qt::ISODate);

    QVERIFY2(CodexCredentialStore::save(path, *credentials, &error), qPrintable(error));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject saved = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject tokens = saved.value(QStringLiteral("tokens")).toObject();
    QCOMPARE(tokens.value(QStringLiteral("access_token")).toString(), QStringLiteral("new-access"));
    QCOMPARE(tokens.value(QStringLiteral("refresh_token")).toString(),
             QStringLiteral("new-refresh"));
    QCOMPARE(tokens.value(QStringLiteral("id_token")).toString(), QStringLiteral("new-id"));
    QCOMPARE(tokens.value(QStringLiteral("account_id")).toString(), QStringLiteral("account"));
    QCOMPARE(tokens.value(QStringLiteral("extra")).toString(), QStringLiteral("keep"));
    QCOMPARE(saved.value(QStringLiteral("unknown")).toInt(), 7);
    QCOMPARE(saved.value(QStringLiteral("last_refresh")).toString(),
             QStringLiteral("2026-09-03T15:00:00.000Z"));
    QCOMPARE(file.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                                   QFileDevice::ReadOther | QFileDevice::WriteOther),
             QFileDevice::Permissions{});
}

void CodexCredentialsTest::rejectsInvalidRotation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("auth.json"));
    QVERIFY(writePrivateFile(path, R"({"OPENAI_API_KEY":"sk-example"})"));
    QString error;
    const auto credentials = CodexCredentialStore::load(path, &error);
    QVERIFY(credentials.has_value());

    QVERIFY(!CodexCredentialStore::save(path, *credentials, &error));
    QCOMPARE(error, QStringLiteral("Codex API-key credentials cannot be rotated"));
    QVERIFY(!CodexCredentialStore::save(directory.filePath(QStringLiteral("missing")), *credentials,
                                        &error));
    QCOMPARE(error, QStringLiteral("Codex auth file was not found"));
}

void CodexCredentialsTest::calculatesRefreshNeed()
{
    const QDateTime now = QDateTime::fromSecsSinceEpoch(1'700'000'000, QTimeZone::UTC);
    CodexCredentials credentials;

    credentials.apiKey = true;
    QVERIFY(!credentials.needsRefresh(now));

    credentials.apiKey = false;
    credentials.expiresAt = now.addSecs(301);
    QVERIFY(!credentials.needsRefresh(now));
    credentials.expiresAt = now.addSecs(300);
    QVERIFY(credentials.needsRefresh(now));

    credentials.expiresAt = {};
    credentials.lastRefresh = now.addDays(-7);
    QVERIFY(!credentials.needsRefresh(now));
    credentials.lastRefresh = now.addDays(-9);
    QVERIFY(credentials.needsRefresh(now));
    credentials.lastRefresh = {};
    QVERIFY(credentials.needsRefresh(now));
}

void CodexCredentialsTest::redactsIdentity()
{
    CodexCredentials credentials;
    credentials.email = QStringLiteral("person@example.com");
    QCOMPARE(credentials.redactedEmail(), QStringLiteral("redacted@example.com"));
    credentials.email = QStringLiteral("opaque");
    QCOMPARE(credentials.redactedEmail(), QStringLiteral("redacted"));
    credentials.email.clear();
    QVERIFY(credentials.redactedEmail().isEmpty());
}

QTEST_GUILESS_MAIN(CodexCredentialsTest)

#include "tst_codex_credentials.moc"
