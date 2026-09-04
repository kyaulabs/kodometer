#include <kodometer/kimi_credentials.hpp>

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace Kodometer;

namespace {

QString writeCredential(const QString &home, const QJsonObject &object,
                        QFileDevice::Permissions permissions = QFileDevice::ReadOwner |
                                                               QFileDevice::WriteOwner)
{
    const QString directory = home + QStringLiteral("/credentials");
    QDir().mkpath(directory);
    const QString path = directory + QStringLiteral("/kimi-code.json");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    file.close();
    QFile::setPermissions(path, permissions);
    return path;
}

QJsonObject credential(const QJsonValue &expiry = 1'800'003'600.0)
{
    return {{QStringLiteral("access_token"), QStringLiteral("cli-access")},
            {QStringLiteral("refresh_token"), QStringLiteral("unused-refresh")},
            {QStringLiteral("expires_at"), expiry},
            {QStringLiteral("preserved"), true}};
}

} // namespace

class KimiCredentialsTest final : public QObject
{
    Q_OBJECT

  private slots:
    void resolvesPaths();
    void parsesCredentialVariants();
    void rejectsMalformedCredentials();
    void prefersExplicitApiKey();
    void securelyLoadsCliCredentialAndCreatesDeviceId();
    void rejectsExpiredAndInsecureFiles();
};

void KimiCredentialsTest::resolvesPaths()
{
    QCOMPARE(KimiCredentialStore::codeHomePath({}, QStringLiteral("/home/test")),
             QStringLiteral("/home/test/.kimi-code"));
    QCOMPARE(KimiCredentialStore::codeHomePath(
                 {{QStringLiteral("KIMI_CODE_HOME"), QStringLiteral("/profiles/kimi")}},
                 QStringLiteral("/home/test")),
             QStringLiteral("/profiles/kimi"));
    QCOMPARE(KimiCredentialStore::codeHomePath(
                 {{QStringLiteral("KIMI_CODE_HOME"), QStringLiteral("relative/kimi")}},
                 QStringLiteral("/home/test"), QStringLiteral("/work")),
             QStringLiteral("/work/relative/kimi"));
    QCOMPARE(KimiCredentialStore::authenticationFilePath(
                 {{QStringLiteral("KIMI_CODE_HOME"), QStringLiteral("/profiles/kimi")}}),
             QStringLiteral("/profiles/kimi/credentials/kimi-code.json"));
    QCOMPARE(KimiCredentialStore::deviceFilePath(
                 {{QStringLiteral("KIMI_CODE_HOME"), QStringLiteral("/profiles/kimi")}}),
             QStringLiteral("/profiles/kimi/device_id"));
}

void KimiCredentialsTest::parsesCredentialVariants()
{
    QString error;
    auto parsed = KimiCredentialStore::parse(
        QJsonDocument(credential(QStringLiteral("1800003600"))).toJson(), &error);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->accessToken, QStringLiteral("cli-access"));
    QCOMPARE(parsed->source, KimiCredentialSource::Cli);
    QCOMPARE(parsed->expiresAt.toSecsSinceEpoch(), 1'800'003'600);
    QVERIFY(error.isEmpty());

    parsed =
        KimiCredentialStore::parse(R"({"access_token":" token ","expires_at":1800007200})", &error);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->accessToken, QStringLiteral("token"));
    QCOMPARE(parsed->expiresAt.toSecsSinceEpoch(), 1'800'007'200);
}

void KimiCredentialsTest::rejectsMalformedCredentials()
{
    const QList<QPair<QByteArray, QString>> cases{
        {"{", QStringLiteral("Kimi Code credentials contain invalid JSON")},
        {"[]", QStringLiteral("Kimi Code credentials must contain a JSON object")},
        {R"({"expires_at":1800003600})", QStringLiteral("Kimi Code CLI access token is missing")},
        {R"({"access_token":"token"})",
         QStringLiteral("Kimi Code CLI expiry is missing or invalid")},
        {R"({"access_token":"token","expires_at":"invalid"})",
         QStringLiteral("Kimi Code CLI expiry is missing or invalid")},
        {R"({"access_token":"token","expires_at":-1})",
         QStringLiteral("Kimi Code CLI expiry is missing or invalid")},
    };

    for (const auto &[data, expected] : cases) {
        QString error;
        QVERIFY(!KimiCredentialStore::parse(data, &error).has_value());
        QCOMPARE(error, expected);
    }
}

void KimiCredentialsTest::prefersExplicitApiKey()
{
    QString error;
    const auto resolved = KimiCredentialStore::resolve(
        {{QStringLiteral("KIMI_CODE_API_KEY"), QStringLiteral("  'api-key'  ")},
         {QStringLiteral("KIMI_CODE_HOME"), QStringLiteral("/missing")}},
        {}, {}, QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC), &error);

    QVERIFY(resolved.has_value());
    QCOMPARE(resolved->source, KimiCredentialSource::ApiKey);
    QCOMPARE(resolved->accessToken, QStringLiteral("api-key"));
    QVERIFY(resolved->deviceId.isEmpty());
    QVERIFY(error.isEmpty());
}

void KimiCredentialsTest::securelyLoadsCliCredentialAndCreatesDeviceId()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString home = temporary.path() + QStringLiteral("/kimi");
    QVERIFY(!writeCredential(home, credential()).isEmpty());
    const QMap<QString, QString> environment{{QStringLiteral("KIMI_CODE_HOME"), home}};
    const QDateTime now = QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC);

    QString error;
    auto resolved = KimiCredentialStore::resolve(environment, {}, {}, now, &error);
    QVERIFY(resolved.has_value());
    QCOMPARE(resolved->source, KimiCredentialSource::Cli);
    QCOMPARE(resolved->accessToken, QStringLiteral("cli-access"));
    QVERIFY(!resolved->deviceId.isEmpty());
    QVERIFY(error.isEmpty());

    const QString devicePath = KimiCredentialStore::deviceFilePath(environment);
    QFileInfo deviceInfo(devicePath);
    QVERIFY(deviceInfo.isFile());
    QCOMPARE(deviceInfo.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                                         QFileDevice::ReadOther | QFileDevice::WriteOther),
             QFileDevice::Permissions{});

    const QString firstDeviceId = resolved->deviceId;
    resolved = KimiCredentialStore::resolve(environment, {}, {}, now, &error);
    QVERIFY(resolved.has_value());
    QCOMPARE(resolved->deviceId, firstDeviceId);
}

void KimiCredentialsTest::rejectsExpiredAndInsecureFiles()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QDateTime now = QDateTime::fromSecsSinceEpoch(1'800'000'000, QTimeZone::UTC);

    const QString expiredHome = temporary.path() + QStringLiteral("/expired");
    QVERIFY(!writeCredential(expiredHome, credential(1'800'000'030.0)).isEmpty());
    QString error;
    QVERIFY(!KimiCredentialStore::resolve({{QStringLiteral("KIMI_CODE_HOME"), expiredHome}}, {}, {},
                                          now, &error)
                 .has_value());
    QCOMPARE(error, QStringLiteral("Kimi Code CLI credential is expired; sign in again or set "
                                   "KIMI_CODE_API_KEY"));

    const QString exposedHome = temporary.path() + QStringLiteral("/exposed");
    const QString exposedPath =
        writeCredential(exposedHome, credential(),
                        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup);
    QVERIFY(!exposedPath.isEmpty());
    QVERIFY(!KimiCredentialStore::load(exposedPath, &error).has_value());
    QCOMPARE(error, QStringLiteral("Kimi Code credential permissions expose secrets"));

    const QString symlinkHome = temporary.path() + QStringLiteral("/symlink");
    const QString target = writeCredential(symlinkHome, credential());
    const QString link = temporary.path() + QStringLiteral("/credential-link");
    QVERIFY(QFile::link(target, link));
    QVERIFY(!KimiCredentialStore::load(link, &error).has_value());
    QCOMPARE(error, QStringLiteral("Kimi Code credentials must not be a symbolic link"));

    const QString oversized = temporary.path() + QStringLiteral("/oversized.json");
    QFile oversizedFile(oversized);
    QVERIFY(oversizedFile.open(QIODevice::WriteOnly));
    QCOMPARE(oversizedFile.write(QByteArray(KimiCredentialStore::MaximumFileSize + 1, 'x')),
             KimiCredentialStore::MaximumFileSize + 1);
    oversizedFile.close();
    QFile::setPermissions(oversized, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QVERIFY(!KimiCredentialStore::load(oversized, &error).has_value());
    QCOMPARE(error, QStringLiteral("Kimi Code credentials exceed the 1 MiB limit"));

    QVERIFY(!KimiCredentialStore::load(temporary.path() + QStringLiteral("/missing"), &error)
                 .has_value());
    QCOMPARE(error, QStringLiteral("Kimi Code credentials were not found"));
    QVERIFY(!KimiCredentialStore::load(temporary.path(), &error).has_value());
    QCOMPARE(error, QStringLiteral("Kimi Code credential path is not a regular file"));
}

QTEST_GUILESS_MAIN(KimiCredentialsTest)
#include "tst_kimi_credentials.moc"
