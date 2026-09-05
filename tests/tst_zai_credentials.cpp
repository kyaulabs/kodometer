#include <kodometer/zai_credentials.hpp>

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Kodometer;

namespace {

void writeKey(const QString &path, const QByteArray &value, QFile::Permissions permissions)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(value), value.size());
    file.close();
    QVERIFY(QFile::setPermissions(path, permissions));
}

} // namespace

class ZaiCredentialsTest final : public QObject
{
    Q_OBJECT

  private slots:
    void resolvesGlobalCredential();
    void resolvesChinaAliasesInOrder();
    void resolvesSecureChinaCredentialFile();
    void rejectsUnsafeCredentialFiles();
    void validatesRegionScopeAndTeamContext();
    void rejectsMissingAndUnsafeTokens();
};

void ZaiCredentialsTest::resolvesGlobalCredential()
{
    QString error;
    const auto credentials = ZaiCredentialResolver::resolve(
        {{QStringLiteral("Z_AI_API_KEY"), QStringLiteral("  'global-key'  ")}}, {}, &error);
    QVERIFY(credentials.has_value());
    QVERIFY(error.isEmpty());
    QCOMPARE(credentials->apiKey, QStringLiteral("global-key"));
    QCOMPARE(credentials->region, ZaiRegion::Global);
    QCOMPARE(credentials->scope, ZaiUsageScope::Personal);
    QCOMPARE(credentials->source, ZaiCredentialSource::Environment);
    QVERIFY(credentials->organizationId.isEmpty());
    QVERIFY(credentials->projectId.isEmpty());
}

void ZaiCredentialsTest::resolvesChinaAliasesInOrder()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QMap<QString, QString> environment{
        {QStringLiteral("Z_AI_REGION"), QStringLiteral("bigmodel-cn")},
        {QStringLiteral("BIGMODEL_API_KEY"), QStringLiteral("bigmodel")},
        {QStringLiteral("ZHIPU_API_KEY"), QStringLiteral("zhipu")},
        {QStringLiteral("ZHIPUAI_API_KEY"), QStringLiteral("zhipuai")},
        {QStringLiteral("GLM_API_KEY"), QStringLiteral("glm")},
    };
    auto credentials = ZaiCredentialResolver::resolve(environment, home.path());
    QVERIFY(credentials.has_value());
    QCOMPARE(credentials->apiKey, QStringLiteral("bigmodel"));
    QCOMPARE(credentials->region, ZaiRegion::BigModelChina);

    QMap<QString, QString> fallback = environment;
    fallback.remove(QStringLiteral("BIGMODEL_API_KEY"));
    credentials = ZaiCredentialResolver::resolve(fallback, home.path());
    QVERIFY(credentials.has_value());
    QCOMPARE(credentials->apiKey, QStringLiteral("zhipu"));

    fallback.remove(QStringLiteral("ZHIPU_API_KEY"));
    credentials = ZaiCredentialResolver::resolve(fallback, home.path());
    QVERIFY(credentials.has_value());
    QCOMPARE(credentials->apiKey, QStringLiteral("zhipuai"));

    fallback.remove(QStringLiteral("ZHIPUAI_API_KEY"));
    credentials = ZaiCredentialResolver::resolve(fallback, home.path());
    QVERIFY(credentials.has_value());
    QCOMPARE(credentials->apiKey, QStringLiteral("glm"));
}

void ZaiCredentialsTest::resolvesSecureChinaCredentialFile()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    writeKey(home.filePath(QStringLiteral(".config/bigmodel/api_key")), " file-key\nignored\n",
             QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    auto credentials = ZaiCredentialResolver::resolve(
        {{QStringLiteral("Z_AI_REGION"), QStringLiteral("bigmodel-cn")}}, home.path());
    QVERIFY(credentials.has_value());
    QCOMPARE(credentials->apiKey, QStringLiteral("file-key"));
    QCOMPARE(credentials->source, ZaiCredentialSource::CredentialFile);

    QVERIFY(QFile::remove(home.filePath(QStringLiteral(".config/bigmodel/api_key"))));
    writeKey(home.filePath(QStringLiteral(".config/zhipu/api_key")), "zhipu-file",
             QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    credentials = ZaiCredentialResolver::resolve(
        {{QStringLiteral("Z_AI_REGION"), QStringLiteral("bigmodel-cn")}}, home.path());
    QVERIFY(credentials.has_value());
    QCOMPARE(credentials->apiKey, QStringLiteral("zhipu-file"));
}

void ZaiCredentialsTest::rejectsUnsafeCredentialFiles()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString path = home.filePath(QStringLiteral(".config/bigmodel/api_key"));
    const auto environment =
        QMap<QString, QString>{{QStringLiteral("Z_AI_REGION"), QStringLiteral("bigmodel-cn")}};
    QString error;

    writeKey(path, "key",
             QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup);
    QVERIFY(!ZaiCredentialResolver::resolve(environment, home.path(), &error).has_value());
    QCOMPARE(error, QStringLiteral("z.ai credential file permissions are too broad"));

    QVERIFY(QFile::remove(path));
    const QString target = home.filePath(QStringLiteral("target"));
    writeKey(target, "key", QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QVERIFY(QFile::link(target, path));
    QVERIFY(!ZaiCredentialResolver::resolve(environment, home.path(), &error).has_value());
    QCOMPARE(error, QStringLiteral("z.ai credential file must not be a symbolic link"));

    QVERIFY(QFile::remove(path));
    writeKey(path, QByteArray(ZaiCredentialResolver::MaximumCredentialFileSize + 1, 'x'),
             QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QVERIFY(!ZaiCredentialResolver::resolve(environment, home.path(), &error).has_value());
    QCOMPARE(error, QStringLiteral("z.ai credential file exceeds the 1 MiB limit"));

    QVERIFY(QFile::remove(path));
    QVERIFY(QDir().mkpath(path));
    QVERIFY(!ZaiCredentialResolver::resolve(environment, home.path(), &error).has_value());
    QCOMPARE(error, QStringLiteral("z.ai credential path is not a regular file"));

    QVERIFY(QDir(path).removeRecursively());
    writeKey(path, {}, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QVERIFY(!ZaiCredentialResolver::resolve(environment, home.path(), &error).has_value());
    QCOMPARE(error, QStringLiteral("z.ai credential file contains no API key"));

    QVERIFY(QFile::remove(path));
    writeKey(path, "key\rbreak", QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QVERIFY(!ZaiCredentialResolver::resolve(environment, home.path(), &error).has_value());
    QCOMPARE(error, QStringLiteral("z.ai API key contains invalid characters"));
}

void ZaiCredentialsTest::validatesRegionScopeAndTeamContext()
{
    QString error;
    QVERIFY(!ZaiCredentialResolver::resolve(
                 {{QStringLiteral("Z_AI_API_KEY"), QStringLiteral("key")},
                  {QStringLiteral("Z_AI_REGION"), QStringLiteral("unknown")}},
                 {}, &error)
                 .has_value());
    QCOMPARE(error, QStringLiteral("z.ai region must be global or bigmodel-cn"));

    QVERIFY(!ZaiCredentialResolver::resolve(
                 {{QStringLiteral("Z_AI_API_KEY"), QStringLiteral("key")},
                  {QStringLiteral("Z_AI_USAGE_SCOPE"), QStringLiteral("invalid")}},
                 {}, &error)
                 .has_value());
    QCOMPARE(error, QStringLiteral("z.ai usage scope must be personal or team"));

    const QMap<QString, QString> team{
        {QStringLiteral("Z_AI_API_KEY"), QStringLiteral("key")},
        {QStringLiteral("Z_AI_REGION"), QStringLiteral("bigmodel-cn")},
        {QStringLiteral("Z_AI_USAGE_SCOPE"), QStringLiteral("team")}};
    QVERIFY(!ZaiCredentialResolver::resolve(team, {}, &error).has_value());
    QCOMPARE(error,
             QStringLiteral("z.ai team usage requires organization and project identifiers"));

    QMap<QString, QString> complete = team;
    complete.insert(QStringLiteral("Z_AI_BIGMODEL_ORGANIZATION"), QStringLiteral("org-id"));
    complete.insert(QStringLiteral("Z_AI_BIGMODEL_PROJECT"), QStringLiteral("project-id"));
    const auto credentials = ZaiCredentialResolver::resolve(complete, {}, &error);
    QVERIFY(credentials.has_value());
    QCOMPARE(credentials->scope, ZaiUsageScope::Team);
    QCOMPARE(credentials->organizationId, QStringLiteral("org-id"));
    QCOMPARE(credentials->projectId, QStringLiteral("project-id"));

    complete.insert(QStringLiteral("Z_AI_BIGMODEL_PROJECT"), QStringLiteral("bad\nproject"));
    QVERIFY(!ZaiCredentialResolver::resolve(complete, {}, &error).has_value());
    QCOMPARE(error, QStringLiteral("z.ai team identifiers contain invalid characters"));
}

void ZaiCredentialsTest::rejectsMissingAndUnsafeTokens()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QString error;
    QVERIFY(!ZaiCredentialResolver::resolve({}, home.path(), &error).has_value());
    QCOMPARE(error, QStringLiteral("z.ai API key is missing"));

    QVERIFY(!ZaiCredentialResolver::resolve(
                 {{QStringLiteral("BIGMODEL_API_KEY"), QStringLiteral("china-only")}}, home.path(),
                 &error)
                 .has_value());
    QCOMPARE(error, QStringLiteral("z.ai API key is missing"));

    QVERIFY(
        !ZaiCredentialResolver::resolve(
             {{QStringLiteral("Z_AI_API_KEY"), QStringLiteral("bad\rkey")}}, home.path(), &error)
             .has_value());
    QCOMPARE(error, QStringLiteral("z.ai API key contains invalid characters"));
}

QTEST_GUILESS_MAIN(ZaiCredentialsTest)
#include "tst_zai_credentials.moc"
