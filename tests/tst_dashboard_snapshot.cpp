#include <kodometer/dashboard_snapshot.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

using Kodometer::DashboardSnapshot;

class DashboardSnapshotTest final : public QObject
{
    Q_OBJECT

  private slots:
    void parsesDocumentedSchema();
    void reportsInvalidDocuments_data();
    void reportsInvalidDocuments();
    void rejectsOversizedPayload();
    void findsProviders();
    void calculatesStaleness();
};

void DashboardSnapshotTest::parsesDocumentedSchema()
{
    QFile fixture(QStringLiteral(FIXTURE_PATH));
    QVERIFY2(fixture.open(QIODevice::ReadOnly), qPrintable(fixture.errorString()));

    QString error;
    const auto snapshot = DashboardSnapshot::fromJson(fixture.readAll(), &error);

    QVERIFY2(snapshot.has_value(), qPrintable(error));
    QCOMPARE(snapshot->schemaVersion(), 1);
    QCOMPARE(snapshot->generatedAt(),
             QDateTime::fromString(QStringLiteral("2026-07-16T12:00:00Z"), Qt::ISODate));
    QCOMPARE(snapshot->staleAfterSeconds(), 180);
    QCOMPARE(snapshot->providers().size(), 2);
    QCOMPARE(snapshot->toVariantMap().value(QStringLiteral("schemaVersion")).toInt(), 1);

    const auto codex = snapshot->provider(QStringLiteral("codex"));
    QVERIFY(codex.has_value());
    QVERIFY(snapshot->provider(QStringLiteral("claude")).has_value());
    QVERIFY(codex->value(QStringLiteral("futureField"))
                .toObject()
                .value(QStringLiteral("preserved"))
                .toBool());
}

void DashboardSnapshotTest::reportsInvalidDocuments_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<QString>("errorFragment");

    QTest::newRow("empty") << QByteArray() << QStringLiteral("empty");
    QTest::newRow("malformed") << QByteArrayLiteral("{") << QStringLiteral("JSON");
    QTest::newRow("array-root") << QByteArrayLiteral("[]") << QStringLiteral("object");
    QTest::newRow("schema-missing")
        << QByteArrayLiteral(
               R"({"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[]})")
        << QStringLiteral("schemaVersion");
    QTest::newRow("schema-fractional")
        << QByteArrayLiteral(
               R"({"schemaVersion":1.5,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[]})")
        << QStringLiteral("schemaVersion");
    QTest::newRow("schema-unsupported")
        << QByteArrayLiteral(
               R"({"schemaVersion":2,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[]})")
        << QStringLiteral("unsupported");
    QTest::newRow("generated-missing")
        << QByteArrayLiteral(R"({"schemaVersion":1,"staleAfterSeconds":180,"providers":[]})")
        << QStringLiteral("generatedAt");
    QTest::newRow("generated-invalid")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"yesterday","staleAfterSeconds":180,"providers":[]})")
        << QStringLiteral("generatedAt");
    QTest::newRow("stale-missing")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","providers":[]})")
        << QStringLiteral("staleAfterSeconds");
    QTest::newRow("stale-negative")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":-1,"providers":[]})")
        << QStringLiteral("staleAfterSeconds");
    QTest::newRow("stale-too-large")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":2147483648,"providers":[]})")
        << QStringLiteral("staleAfterSeconds");
    QTest::newRow("providers-missing")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180})")
        << QStringLiteral("providers");
    QTest::newRow("provider-not-object")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[null]})")
        << QStringLiteral("provider");
    QTest::newRow("provider-id-empty")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[{"id":"","name":"Codex","enabled":true,"windows":[]}]})")
        << QStringLiteral("id");
    QTest::newRow("provider-id-whitespace")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[{"id":"  ","name":"Codex","enabled":true,"windows":[]}]})")
        << QStringLiteral("id");
    QTest::newRow("provider-id-wrong-type")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[{"id":7,"name":"Codex","enabled":true,"windows":[]}]})")
        << QStringLiteral("id");
    QTest::newRow("provider-name-missing")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[{"id":"codex","enabled":true,"windows":[]}]})")
        << QStringLiteral("name");
    QTest::newRow("provider-name-empty")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[{"id":"codex","name":"  ","enabled":true,"windows":[]}]})")
        << QStringLiteral("name");
    QTest::newRow("provider-enabled-wrong-type")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[{"id":"codex","name":"Codex","enabled":1,"windows":[]}]})")
        << QStringLiteral("enabled");
    QTest::newRow("provider-windows-wrong-type")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[{"id":"codex","name":"Codex","enabled":true,"windows":{}}]})")
        << QStringLiteral("windows");
    QTest::newRow("provider-duplicate")
        << QByteArrayLiteral(
               R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[{"id":"codex","name":"Codex","enabled":true,"windows":[]},{"id":"codex","name":"Codex","enabled":true,"windows":[]}]})")
        << QStringLiteral("duplicate");
}

void DashboardSnapshotTest::reportsInvalidDocuments()
{
    QFETCH(QByteArray, payload);
    QFETCH(QString, errorFragment);

    QString error;
    const auto snapshot = DashboardSnapshot::fromJson(payload, &error);

    QVERIFY(!snapshot.has_value());
    QVERIFY2(error.contains(errorFragment, Qt::CaseInsensitive), qPrintable(error));
}

void DashboardSnapshotTest::rejectsOversizedPayload()
{
    const QByteArray payload(DashboardSnapshot::MaximumPayloadBytes + 1, ' ');
    QString error;

    QVERIFY(!DashboardSnapshot::fromJson(payload, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("large"), Qt::CaseInsensitive));
    QVERIFY(!DashboardSnapshot::fromJson(payload).has_value());
}

void DashboardSnapshotTest::findsProviders()
{
    const QByteArray payload = QByteArrayLiteral(
        R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":0,"providers":[]})");
    const auto snapshot = DashboardSnapshot::fromJson(payload);

    QVERIFY(snapshot.has_value());
    QVERIFY(!snapshot->provider(QStringLiteral("missing")).has_value());
}

void DashboardSnapshotTest::calculatesStaleness()
{
    const QByteArray payload = QByteArrayLiteral(
        R"({"schemaVersion":1,"generatedAt":"2026-01-01T00:00:00Z","staleAfterSeconds":180,"providers":[]})");
    const auto snapshot = DashboardSnapshot::fromJson(payload);
    QVERIFY(snapshot.has_value());

    QVERIFY(!snapshot->isStale(
        QDateTime::fromString(QStringLiteral("2026-01-01T00:03:00Z"), Qt::ISODate)));
    QVERIFY(snapshot->isStale(
        QDateTime::fromString(QStringLiteral("2026-01-01T00:03:01Z"), Qt::ISODate)));
}

QTEST_GUILESS_MAIN(DashboardSnapshotTest)

#include "tst_dashboard_snapshot.moc"
