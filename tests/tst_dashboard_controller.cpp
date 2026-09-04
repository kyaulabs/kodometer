#include <codexbar/dashboard_controller.hpp>

#include <QSignalSpy>
#include <QtTest>

using CodexBar::DashboardController;

class DashboardControllerTest final : public QObject
{
    Q_OBJECT

  private slots:
    void init();
    void cleanup();
    void hasSafeDefaults();
    void refreshesFromCli();
    void ignoresConcurrentRefresh();
    void retainsSnapshotAfterInvalidJson();
    void reportsCommandFailure();
    void reportsMissingExecutable();
    void enforcesOutputLimit();
    void timesOutSlowCommand();

  private:
    static void selectMode(const QByteArray &mode);
    static bool waitForRefresh(DashboardController &controller, bool expectedSuccess);
};

void DashboardControllerTest::init()
{
    selectMode("success");
}

void DashboardControllerTest::cleanup()
{
    qunsetenv("CODEXBAR_TEST_MODE");
}

void DashboardControllerTest::hasSafeDefaults()
{
    DashboardController controller;

    QCOMPARE(controller.executable(), QStringLiteral("codexbar"));
    QCOMPARE(controller.timeoutMilliseconds(), 30'000);
    QVERIFY(controller.identityRedacted());
    QVERIFY(!controller.busy());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(controller.snapshot().isEmpty());
    QVERIFY(controller.providers().isEmpty());
}

void DashboardControllerTest::refreshesFromCli()
{
    DashboardController controller;
    controller.setExecutable(QStringLiteral(FAKE_CODEXBAR_PATH));
    QSignalSpy busySpy(&controller, &DashboardController::busyChanged);
    QSignalSpy snapshotSpy(&controller, &DashboardController::snapshotChanged);

    QVERIFY(waitForRefresh(controller, true));
    QCOMPARE(busySpy.count(), 2);
    QCOMPARE(snapshotSpy.count(), 1);
    QCOMPARE(controller.providers().size(), 2);
    QCOMPARE(controller.snapshot().value(QStringLiteral("schemaVersion")).toInt(), 1);
    QVERIFY(controller.error().isEmpty());
}

void DashboardControllerTest::ignoresConcurrentRefresh()
{
    DashboardController controller;
    controller.setExecutable(QStringLiteral(FAKE_CODEXBAR_PATH));
    selectMode("slow");
    QSignalSpy finishedSpy(&controller, &DashboardController::refreshFinished);

    controller.refresh();
    controller.refresh();

    QVERIFY(finishedSpy.wait(2'000));
    QCOMPARE(finishedSpy.count(), 1);
}

void DashboardControllerTest::retainsSnapshotAfterInvalidJson()
{
    DashboardController controller;
    controller.setExecutable(QStringLiteral(FAKE_CODEXBAR_PATH));
    QVERIFY(waitForRefresh(controller, true));
    const QVariantMap lastGood = controller.snapshot();

    selectMode("malformed");
    QVERIFY(waitForRefresh(controller, false));
    QCOMPARE(controller.snapshot(), lastGood);
    QVERIFY(controller.error().contains(QStringLiteral("JSON"), Qt::CaseInsensitive));
}

void DashboardControllerTest::reportsCommandFailure()
{
    DashboardController controller;
    controller.setExecutable(QStringLiteral(FAKE_CODEXBAR_PATH));
    selectMode("failure");

    QVERIFY(waitForRefresh(controller, false));
    QVERIFY(controller.error().contains(QStringLiteral("provider request failed")));
}

void DashboardControllerTest::reportsMissingExecutable()
{
    DashboardController controller;
    controller.setExecutable(QStringLiteral("/definitely/missing/codexbar"));

    QVERIFY(waitForRefresh(controller, false));
    QVERIFY(controller.error().contains(QStringLiteral("start"), Qt::CaseInsensitive));
}

void DashboardControllerTest::enforcesOutputLimit()
{
    DashboardController controller;
    controller.setExecutable(QStringLiteral(FAKE_CODEXBAR_PATH));
    selectMode("oversized");

    QVERIFY(waitForRefresh(controller, false));
    QVERIFY(controller.error().contains(QStringLiteral("large"), Qt::CaseInsensitive));
}

void DashboardControllerTest::timesOutSlowCommand()
{
    DashboardController controller;
    controller.setExecutable(QStringLiteral(FAKE_CODEXBAR_PATH));
    controller.setTimeoutMilliseconds(25);
    selectMode("slow");

    QVERIFY(waitForRefresh(controller, false));
    QVERIFY(controller.error().contains(QStringLiteral("timed out"), Qt::CaseInsensitive));
}

void DashboardControllerTest::selectMode(const QByteArray &mode)
{
    QVERIFY(qputenv("CODEXBAR_TEST_MODE", mode));
}

bool DashboardControllerTest::waitForRefresh(DashboardController &controller, bool expectedSuccess)
{
    QSignalSpy finishedSpy(&controller, &DashboardController::refreshFinished);
    controller.refresh();
    if (finishedSpy.isEmpty() && !finishedSpy.wait(2'000)) {
        return false;
    }
    return finishedSpy.last().at(0).toBool() == expectedSuccess;
}

QTEST_GUILESS_MAIN(DashboardControllerTest)

#include "tst_dashboard_controller.moc"
