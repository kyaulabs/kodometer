#include <kodometer/claude_credentials.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

using Kodometer::ClaudeCredentials;
using Kodometer::ClaudeCredentialStore;

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

QByteArray credentialDocument(qint64 expiresAt = 1'800'000'000'000)
{
    return QJsonDocument(
               QJsonObject{
                   {QStringLiteral("claudeAiOauth"),
                    QJsonObject{
                        {QStringLiteral("accessToken"), QStringLiteral("access-token")},
                        {QStringLiteral("refreshToken"), QStringLiteral("refresh-token")},
                        {QStringLiteral("expiresAt"), expiresAt},
                        {QStringLiteral("scopes"), QJsonArray{QStringLiteral("user:profile"),
                                                              QStringLiteral("user:inference")}},
                        {QStringLiteral("rateLimitTier"), QStringLiteral("default_claude_max_20x")},
                        {QStringLiteral("subscriptionType"), QStringLiteral("max")},
                        {QStringLiteral("preserved"), 7},
                    }},
                   {QStringLiteral("mcpOAuth"), QJsonObject{{QStringLiteral("preserved"), true}}},
               })
        .toJson();
}

} // namespace

class ClaudeCredentialsTest final : public QObject
{
    Q_OBJECT

  private slots:
    void resolvesCredentialPaths();
    void parsesOAuthCredentials();
    void rejectsInvalidDocuments();
    void enforcesPrivateFiles();
    void savesRotatedCredentialsAtomically();
    void calculatesRefreshNeed();
};

void ClaudeCredentialsTest::resolvesCredentialPaths()
{
    QCOMPARE(ClaudeCredentialStore::authenticationFilePath({}, QStringLiteral("/home/test"),
                                                           QStringLiteral("/work")),
             QStringLiteral("/home/test/.claude/.credentials.json"));
    QCOMPARE(ClaudeCredentialStore::authenticationFilePath(
                 {{QStringLiteral("CLAUDE_CONFIG_DIR"), QStringLiteral("/profiles/claude")}},
                 QStringLiteral("/home/test"), QStringLiteral("/work")),
             QStringLiteral("/profiles/claude/.credentials.json"));
    QCOMPARE(ClaudeCredentialStore::authenticationFilePath(
                 {{QStringLiteral("CLAUDE_CONFIG_DIR"), QStringLiteral("relative/profile")}},
                 QStringLiteral("/home/test"), QStringLiteral("/work")),
             QStringLiteral("/work/relative/profile/.credentials.json"));
    QCOMPARE(
        ClaudeCredentialStore::authenticationFilePath(
            {{QStringLiteral("CLAUDE_CONFIG_DIR"), QStringLiteral("/profiles/claude")},
             {QStringLiteral("CLAUDE_SECURESTORAGE_CONFIG_DIR"), QStringLiteral("/secure/claude")}},
            QStringLiteral("/home/test"), QStringLiteral("/work")),
        QStringLiteral("/secure/claude/.credentials.json"));
    QCOMPARE(ClaudeCredentialStore::authenticationFilePath(
                 {{QStringLiteral("CLAUDE_CONFIG_DIR"), QStringLiteral("/profiles/claude")},
                  {QStringLiteral("CLAUDE_SECURESTORAGE_CONFIG_DIR"), QString()}},
                 QStringLiteral("/home/test"), QStringLiteral("/work")),
             QStringLiteral("/profiles/claude/.credentials.json"));
}

void ClaudeCredentialsTest::parsesOAuthCredentials()
{
    QString error;
    const auto credentials = ClaudeCredentialStore::parse(credentialDocument(), &error);

    QVERIFY2(credentials.has_value(), qPrintable(error));
    QCOMPARE(credentials->accessToken, QStringLiteral("access-token"));
    QCOMPARE(credentials->refreshToken, QStringLiteral("refresh-token"));
    QCOMPARE(credentials->expiresAt,
             QDateTime::fromMSecsSinceEpoch(1'800'000'000'000, QTimeZone::UTC));
    QCOMPARE(credentials->scopes,
             QStringList({QStringLiteral("user:profile"), QStringLiteral("user:inference")}));
    QCOMPARE(credentials->rateLimitTier, QStringLiteral("default_claude_max_20x"));
    QCOMPARE(credentials->subscriptionType, QStringLiteral("max"));

    const auto trimmed = ClaudeCredentialStore::parse(
        R"({"claudeAiOauth":{"accessToken":" token ","refreshToken":" ","scopes":[" one ",4,""],"expiresAt":null}})",
        &error);
    QVERIFY2(trimmed.has_value(), qPrintable(error));
    QCOMPARE(trimmed->accessToken, QStringLiteral("token"));
    QVERIFY(trimmed->refreshToken.isEmpty());
    QCOMPARE(trimmed->scopes, QStringList{QStringLiteral("one")});
    QVERIFY(!trimmed->expiresAt.isValid());
}

void ClaudeCredentialsTest::rejectsInvalidDocuments()
{
    QString error;
    QVERIFY(!ClaudeCredentialStore::parse("{", &error));
    QCOMPARE(error, QStringLiteral("Claude credentials contain invalid JSON"));
    QVERIFY(!ClaudeCredentialStore::parse("[]", &error));
    QCOMPARE(error, QStringLiteral("Claude credentials must contain a JSON object"));
    QVERIFY(!ClaudeCredentialStore::parse(R"({"mcpOAuth":{}})", &error));
    QCOMPARE(error, QStringLiteral("Claude credentials contain no Claude OAuth session"));
    QVERIFY(!ClaudeCredentialStore::parse(R"({"claudeAiOauth":{"accessToken":" "}})", &error));
    QCOMPARE(error, QStringLiteral("Claude OAuth access token is missing"));
    QVERIFY(!ClaudeCredentialStore::parse(
        R"({"claudeAiOauth":{"accessToken":"token","expiresAt":-1}})", &error));
    QCOMPARE(error, QStringLiteral("Claude OAuth expiry is invalid"));
    QVERIFY(!ClaudeCredentialStore::parse("{", nullptr));
}

void ClaudeCredentialsTest::enforcesPrivateFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString error;
    const QString path = directory.filePath(QStringLiteral(".credentials.json"));

    QVERIFY(!ClaudeCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Claude credentials were not found"));
    QVERIFY(writePrivateFile(path, credentialDocument()));
    QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                            QFileDevice::ReadGroup));
    QVERIFY(!ClaudeCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Claude credential permissions expose secrets"));

    QVERIFY(QFile::remove(path));
    QVERIFY(QFile::link(directory.filePath(QStringLiteral("missing")), path));
    QVERIFY(!ClaudeCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Claude credentials must not be a symbolic link"));

    QVERIFY(QFile::remove(path));
    QVERIFY(writePrivateFile(path, QByteArray(ClaudeCredentialStore::MaximumFileSize + 1, 'x')));
    QVERIFY(!ClaudeCredentialStore::load(path, &error));
    QCOMPARE(error, QStringLiteral("Claude credentials exceed the 1 MiB limit"));
}

void ClaudeCredentialsTest::savesRotatedCredentialsAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral(".credentials.json"));
    QVERIFY(writePrivateFile(path, credentialDocument()));
    QString error;
    auto credentials = ClaudeCredentialStore::load(path, &error);
    QVERIFY2(credentials.has_value(), qPrintable(error));

    credentials->accessToken = QStringLiteral("new-access");
    credentials->refreshToken = QStringLiteral("new-refresh");
    credentials->expiresAt = QDateTime::fromMSecsSinceEpoch(1'900'000'000'000, QTimeZone::UTC);
    credentials->scopes = {QStringLiteral("user:profile")};
    credentials->rateLimitTier = QStringLiteral("pro");
    credentials->subscriptionType = QStringLiteral("pro");
    QVERIFY2(ClaudeCredentialStore::save(path, *credentials, &error), qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject oauth = root.value(QStringLiteral("claudeAiOauth")).toObject();
    QCOMPARE(oauth.value(QStringLiteral("accessToken")).toString(), QStringLiteral("new-access"));
    QCOMPARE(oauth.value(QStringLiteral("refreshToken")).toString(), QStringLiteral("new-refresh"));
    QCOMPARE(oauth.value(QStringLiteral("expiresAt")).toVariant().toLongLong(), 1'900'000'000'000);
    QCOMPARE(oauth.value(QStringLiteral("preserved")).toInt(), 7);
    QVERIFY(root.contains(QStringLiteral("mcpOAuth")));
    QCOMPARE(file.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                                   QFileDevice::ReadOther | QFileDevice::WriteOther),
             QFileDevice::Permissions{});

    QVERIFY(!ClaudeCredentialStore::save(directory.filePath(QStringLiteral("missing")),
                                         *credentials, &error));
    QCOMPARE(error, QStringLiteral("Claude credentials were not found"));
}

void ClaudeCredentialsTest::calculatesRefreshNeed()
{
    const QDateTime now = QDateTime::fromSecsSinceEpoch(1'700'000'000, QTimeZone::UTC);
    ClaudeCredentials credentials;

    QVERIFY(credentials.needsRefresh(now));
    credentials.expiresAt = now.addSecs(301);
    QVERIFY(!credentials.needsRefresh(now));
    credentials.expiresAt = now.addSecs(300);
    QVERIFY(credentials.needsRefresh(now));
}

QTEST_GUILESS_MAIN(ClaudeCredentialsTest)

#include "tst_claude_credentials.moc"
