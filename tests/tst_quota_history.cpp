#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include <cerrno>
#include <fcntl.h>
#include <kodometer/quota_history.hpp>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

using Kodometer::QuotaHistory;

namespace {
enum class Fault
{
    None,
    DirectoryStat,
    FileStat,
    DirectoryOwner,
    FileOwner,
    GrowingFile,
    Sync,
    Rename
};
Fault activeFault = Fault::None;
} // namespace

extern "C" int __real_fstat(int fd, struct stat *info);
extern "C" int __real_fsync(int fd);
extern "C" int __real_renameat(int oldDirectory, const char *oldPath, int newDirectory,
                               const char *newPath);

extern "C" int __wrap_fstat(int fd, struct stat *info)
{
    const int result = __real_fstat(fd, info);
    if (result == 0 && ((activeFault == Fault::DirectoryStat && S_ISDIR(info->st_mode)) ||
                        (activeFault == Fault::FileStat && S_ISREG(info->st_mode)))) {
        errno = EIO;
        return -1;
    }
    if (result == 0 && ((activeFault == Fault::DirectoryOwner && S_ISDIR(info->st_mode)) ||
                        (activeFault == Fault::FileOwner && S_ISREG(info->st_mode))))
        info->st_uid = ::getuid() + 1;
    if (result == 0 && activeFault == Fault::GrowingFile && S_ISREG(info->st_mode))
        info->st_size = 0;
    return result;
}
extern "C" int __wrap_fsync(int fd)
{
    if (activeFault == Fault::Sync) {
        errno = EIO;
        return -1;
    }
    return __real_fsync(fd);
}
extern "C" int __wrap_renameat(int oldDirectory, const char *oldPath, int newDirectory,
                               const char *newPath)
{
    if (activeFault == Fault::Rename) {
        errno = EIO;
        return -1;
    }
    return __real_renameat(oldDirectory, oldPath, newDirectory, newPath);
}

class QuotaHistoryTest : public QObject
{
    Q_OBJECT
  private:
    static QVariantList quota(double remaining = 65)
    {
        return {QVariantMap{{"kind", "session"}, {"remainingPercent", remaining}}};
    }
    static QVariantList points(const QuotaHistory &history)
    {
        const auto view = history.view("openai");
        return view.isEmpty() ? QVariantList{} : view.first().toMap().value("points").toList();
    }
    static void writeFile(const QString &path, const QByteArray &bytes)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner));
        QCOMPARE(file.write(bytes), bytes.size());
    }

  private slots:
    void boundsSamplesAndRotatesOnlyTrustedIdentities()
    {
        QuotaHistory history;
        const auto now = QDateTime::fromSecsSinceEpoch(1800000000, QTimeZone::UTC);
        QVERIFY(!history.persistent());
        history.setPersistent(false);
        history.observe("openai", "a", {"old"}, quota(), now);
        history.observe("openai", "a", {"old", "new"}, quota(60), now.addSecs(600));
        QCOMPARE(points(history).size(), 2);
        history.observe("openai", "a", {"new"}, quota(55), now.addSecs(610));
        QCOMPARE(points(history).size(), 2);
        QCOMPARE(points(history).last().toList()[1].toDouble(), 55.0);
        history.observe("openai", "a", {"new"}, quota(10), now.addSecs(609));
        QCOMPARE(points(history).last().toList()[1].toDouble(), 55.0);
        history.observe("openai", "a", {"new"}, quota(50), now.addSecs(300));
        QCOMPARE(points(history).size(), 2);
        history.observe("openai", "a", {"new"}, quota(40), now.addDays(31));
        QCOMPARE(points(history).size(), 1);
        history.observe("openai", "a", {"unrelated"}, quota(), now.addDays(31));
        QCOMPARE(points(history).size(), 1);
        history.forgetProvider("missing");
        const int revision = history.revision();
        history.forgetProvider("openai");
        QTRY_VERIFY(history.revision() > revision);
        QVERIFY(history.view("openai").isEmpty());
        QVERIFY(history.clear());
        QVERIFY(history.error().isEmpty());
    }

    void preservesOriginalDataOnFilesystemFailure_data()
    {
        QTest::addColumn<int>("fault");
        QTest::newRow("directory-stat") << static_cast<int>(Fault::DirectoryStat);
        QTest::newRow("file-stat") << static_cast<int>(Fault::FileStat);
        QTest::newRow("directory-owner") << static_cast<int>(Fault::DirectoryOwner);
        QTest::newRow("file-owner") << static_cast<int>(Fault::FileOwner);
        QTest::newRow("sync") << static_cast<int>(Fault::Sync);
        QTest::newRow("rename") << static_cast<int>(Fault::Rename);
    }

    void preservesOriginalDataOnFilesystemFailure()
    {
        QFETCH(int, fault);
        const auto resetFault = qScopeGuard([] { activeFault = Fault::None; });
        QTemporaryDir dir;
        QuotaHistory history;
        history.setDirectory(dir.path());
        history.setPersistent(true);
        const auto now = QDateTime::currentDateTimeUtc();
        history.observe("openai", "scope", {"key"}, quota(), now);
        QVERIFY(history.error().isEmpty());
        QFile original(dir.filePath("history.json"));
        QVERIFY(original.open(QIODevice::ReadOnly));
        const QByteArray before = original.readAll();
        original.close();
        activeFault = static_cast<Fault>(fault);
        history.observe("openai", "scope", {"key"}, quota(50), now.addSecs(600));
        QVERIFY(!history.error().isEmpty());
        activeFault = Fault::None;
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), before);
        original.close();
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files),
                 QStringList{QStringLiteral("history.json")});
        history.observe("openai", "scope", {"key"}, quota(50), now.addSecs(600));
        QVERIFY(history.error().isEmpty());
        QCOMPARE(points(history).size(), 2);
    }

    void migratesLegacyPartitionOnlyAfterMatchingRefresh()
    {
        QTemporaryDir dir;
        QuotaHistory history;
        history.setDirectory(dir.path());
        history.setPersistent(true);
        const auto now = QDateTime::currentDateTimeUtc();
        history.observe("openai", "scope", {"key"}, quota(70), now.addSecs(-600));
        QFile file(dir.filePath("history.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto document = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        auto streams = document.value("streams").toObject();
        QByteArray input("codex");
        input.append('\0');
        input.append("scope");
        input.append('\0');
        input.append("key");
        const QString legacy = QString::fromLatin1(
            QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex());
        const auto saved = streams.begin().value();
        streams = {{legacy, saved}};
        document.insert("streams", streams);
        writeFile(dir.filePath("history.json"), QJsonDocument(document).toJson());
        QuotaHistory migrated;
        migrated.setDirectory(dir.path());
        migrated.setPersistent(true);
        QVERIFY(migrated.view("openai").isEmpty());
        migrated.observe("openai", "scope", {"other"}, quota(30), now);
        QCOMPARE(points(migrated).size(), 1);
        migrated.observe("openai", "scope", {"key"}, quota(60), now);
        QVERIFY2(migrated.error().isEmpty(), qPrintable(migrated.error()));
        QCOMPARE(points(migrated).size(), 2);
    }

    void boundsReadsEvenIfTheFileGrowsAfterStat()
    {
        const auto resetFault = qScopeGuard([] { activeFault = Fault::None; });
        QTemporaryDir dir;
        writeFile(dir.filePath("history.json"), QByteArray(8 * 1024 * 1024 + 1, ' '));
        QuotaHistory history;
        history.setDirectory(dir.path());
        history.setPersistent(true);
        activeFault = Fault::GrowingFile;
        history.observe("openai", "scope", {"key"}, quota());
        QVERIFY(!history.error().isEmpty());
        QVERIFY(history.view("openai").isEmpty());
    }

    void permitsReentrantClearWithoutRestoringData_data()
    {
        QTest::addColumn<bool>("malformed");
        QTest::newRow("saved-observation") << false;
        QTest::newRow("read-failure") << true;
    }

    void permitsReentrantClearWithoutRestoringData()
    {
        QFETCH(bool, malformed);
        QTemporaryDir dir;
        if (malformed)
            writeFile(dir.filePath("history.json"), "invalid");
        QuotaHistory history;
        history.setDirectory(dir.path());
        history.setPersistent(true);
        bool cleared = false;
        connect(
            &history, &QuotaHistory::changed, &history, [&] { cleared = history.clear(); },
            Qt::SingleShotConnection);
        history.observe("openai", "scope", {"key"}, quota());
        QVERIFY(cleared);
        QVERIFY(history.error().isEmpty());
        QVERIFY(history.view("openai").isEmpty());
        QVERIFY(!QFile::exists(dir.filePath("history.json")));
    }

    void clearsAnOldIdentityEvenWithoutUsableObservations()
    {
        QuotaHistory history;
        history.observe("openai", "scope", {"old-key"}, quota());
        QCOMPARE(history.view("openai").size(), 1);
        history.observe("openai", "scope", {"new-key"}, {});
        QVERIFY(history.view("openai").isEmpty());
        history.observe("openai", "scope", {"old-key"}, quota());
        QCOMPARE(history.view("openai").size(), 1);
        history.observe("openai", "scope", {}, quota());
        QVERIFY(history.view("openai").isEmpty());
    }

    void mergesLinkedStreamsFromMultipleWidgets_data()
    {
        QTest::addColumn<int>("offset");
        QTest::addColumn<double>("expected");
        QTest::newRow("older") << -300 << 45.0;
        QTest::newRow("newer") << -180 << 35.0;
        QTest::newRow("equal-time") << -240 << 35.0;
    }

    void mergesLinkedStreamsFromMultipleWidgets()
    {
        QFETCH(int, offset);
        QFETCH(double, expected);
        QTemporaryDir dir;
        QuotaHistory first;
        QuotaHistory second;
        for (auto *history : {&first, &second}) {
            history->setDirectory(dir.path());
            history->setPersistent(true);
        }
        const auto now = QDateTime::fromSecsSinceEpoch(
            QDateTime::currentSecsSinceEpoch() / 300 * 300, QTimeZone::UTC);
        first.observe("openai", "scope", {"old"}, quota(65), now.addSecs(-1200));
        second.observe("openai", "scope", {"new"}, quota(55), now.addSecs(-600));
        first.observe("openai", "scope", {"old"}, quota(45), now.addSecs(-240));
        second.observe("openai", "scope", {"new"}, quota(35), now.addSecs(offset));
        first.observe("openai", "scope", {"old", "new"}, quota(25), now);
        QVERIFY(first.error().isEmpty());
        const auto rows = points(first);
        QCOMPARE(rows.size(), 4);
        QCOMPARE(rows.at(0).toList().at(1).toDouble(), 65.0);
        QCOMPARE(rows.at(1).toList().at(1).toDouble(), 55.0);
        QCOMPARE(rows.at(2).toList().at(1).toDouble(), expected);
        QCOMPARE(rows.at(3).toList().at(1).toDouble(), 25.0);
    }

    void defaultStorageAndWriteFailure()
    {
        QTemporaryDir dir;
        const QByteArray original = qgetenv("XDG_DATA_HOME");
        qputenv("XDG_DATA_HOME", dir.path().toUtf8());
        {
            QuotaHistory history;
            history.setPersistent(true);
            history.setPersistent(true);
            history.observe("openai", "a", {"key"}, quota());
            QVERIFY2(history.error().isEmpty(), qPrintable(history.error()));
            QVERIFY(QFile::exists(dir.filePath("kodometer-quota-history/history.json")));
            history.setPersistent(false);
            QVERIFY(!history.persistent());
        }
        writeFile(dir.filePath("blocked"), "file");
        qputenv("XDG_DATA_HOME", dir.filePath("blocked/child").toUtf8());
        {
            QuotaHistory history;
            history.setPersistent(true);
            history.observe("openai", "a", {"key"}, quota());
            QVERIFY(!history.error().isEmpty());
        }
        if (original.isNull())
            qunsetenv("XDG_DATA_HOME");
        else
            qputenv("XDG_DATA_HOME", original);

        QuotaHistory readonly;
        readonly.setDirectory(QStringLiteral("/proc/self/fd"));
        readonly.setPersistent(true);
        readonly.observe("openai", "a", {"key"}, quota());
        QCOMPARE(readonly.error(),
                 QStringLiteral("Quota history could not be saved. Usage is unaffected."));
        QCOMPARE(points(readonly).size(), 1);
    }

    void boundsStreamsAndPrunesPersistedHistory()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        QuotaHistory memory;
        for (int i = 0; i < 65; ++i)
            memory.observe("openai", QByteArray::number(i), {"key"}, quota(), now.addSecs(i));
        QCOMPARE(points(memory).size(), 1);
        memory.observe("openai", "0", {"key"}, quota(), now.addSecs(600));
        QCOMPARE(points(memory).size(), 1); // The oldest stream was evicted.

        QTemporaryDir dir;
        const QString key(64, 'a');
        const double seconds = static_cast<double>(now.toSecsSinceEpoch());
        const QJsonArray old{QJsonArray{seconds - 31 * 86400, 65, 0}};
        const QJsonObject series{{"session", old}, {"weekly", QJsonArray{}}};
        const QJsonObject streams{{key, series}};
        writeFile(dir.filePath("history.json"),
                  QJsonDocument(QJsonObject{{"version", 1}, {"streams", streams}}).toJson());
        QuotaHistory disk;
        disk.setDirectory(dir.path());
        disk.setPersistent(true);
        disk.observe("openai", "a", {"key"}, quota(), now);
        QVERIFY(disk.error().isEmpty());
        QFile file(dir.filePath("history.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().contains(key.toUtf8()));
    }

    void clearsAbsentFileButRejectsDirectory()
    {
        QTemporaryDir dir;
        QuotaHistory history;
        history.setDirectory(dir.path());
        history.setPersistent(true);
        QVERIFY(history.clear());
        QVERIFY(QDir().mkdir(dir.filePath("history.json")));
        QVERIFY(!history.clear());
        QCOMPARE(history.error(), QStringLiteral("Quota history could not be cleared."));
    }

    void boundsTotalObservations()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        const double seconds = static_cast<double>(now.toSecsSinceEpoch());
        QJsonArray many;
        for (int i = 0; i < 32000; ++i)
            many.append(QJsonArray{seconds - 64000 + i, 65, 0});
        const QJsonObject series{{"session", many}};
        QJsonObject streams{{QString(64, 'a'), series}, {QString(64, 'b'), series}};
        QTemporaryDir dir;
        writeFile(dir.filePath("history.json"),
                  QJsonDocument(QJsonObject{{"version", 1}, {"streams", streams}})
                      .toJson(QJsonDocument::Compact));
        QuotaHistory history;
        history.setDirectory(dir.path());
        history.setPersistent(true);
        history.observe("openai", "a", {"key"}, quota(), now);
        QVERIFY2(history.error().isEmpty(), qPrintable(history.error()));
        QCOMPARE(points(history).size(), 1);
        streams.insert(QString(64, 'c'),
                       QJsonObject{{"session", QJsonArray{QJsonArray{seconds, 65, 0}}}});
        writeFile(dir.filePath("history.json"),
                  QJsonDocument(QJsonObject{{"version", 1}, {"streams", streams}})
                      .toJson(QJsonDocument::Compact));
        history.observe("openai", "a", {"key"}, quota(), now);
        QVERIFY(!history.error().isEmpty());
    }

    void boundsChangingWindowCatalog()
    {
        QuotaHistory history;
        const auto now = QDateTime::currentDateTimeUtc();
        QVariantList windows;
        for (int i = 0; i < 32; ++i)
            windows.append(
                QVariantMap{{"kind", QStringLiteral("model-%1").arg(i)}, {"remainingPercent", 50}});
        history.observe("openai", "a", {"key"}, windows, now);
        QCOMPARE(history.view("openai").size(), 32);
        history.observe("openai", "a", {"key"}, quota(), now.addSecs(600));
        QCOMPARE(history.view("openai").size(), 32);
    }

    void ignoresInvalidObservations()
    {
        QuotaHistory history;
        const auto now = QDateTime::currentDateTimeUtc();
        history.observe("deepseek", "a", {"key"}, quota(), now);
        history.observe("openai", "a", {"key"},
                        {QVariantMap{{"kind", true}, {"remainingPercent", 50}}}, now);
        history.observe("openai", "a", {"key"},
                        {QVariantMap{{"kind", "session\n"}, {"remainingPercent", 50}}}, now);
        history.observe("claude", "a", {"key"},
                        {QVariantMap{{"kind", "spend-limit"}, {"remainingPercent", 50}}}, now);
        QVERIFY(history.view("claude").isEmpty());
        history.observe("openai", "a", {}, quota(), now);
        history.observe("openai", "a", {""}, quota(), now);
        history.observe("openai", "a", {"key"}, quota(), {});
        history.observe("openai", "a", {"key"}, quota(), QDateTime::fromSecsSinceEpoch(0));
        history.observe("openai", "a", {"key"}, QVariantList{quota().first(), quota().first()},
                        now);
        QVariantList tooMany;
        for (int i = 0; i < 33; ++i)
            tooMany.append(quota().first());
        history.observe("openai", "a", {"key"}, tooMany, now);
        for (const QVariant &value :
             {QVariant{}, QVariant("65"), QVariant(true), QVariant(-1.0), QVariant(101.0),
              QVariant(std::numeric_limits<double>::infinity()),
              QVariant(std::numeric_limits<double>::quiet_NaN())}) {
            history.observe("openai", "a", {"key"},
                            {QVariantMap{{"kind", "session"}, {"remainingPercent", value}}}, now);
        }
        history.observe("openai", "a", {"key"},
                        {QVariantMap{{"kind", "../private"}, {"remainingPercent", 50}}}, now);
        history.observe(
            "openai", "a", {"key"},
            {QVariantMap{{"kind", "session"}, {"idle", true}, {"remainingPercent", 50}}}, now);
        QVERIFY(history.view("openai").isEmpty());
        history.observe("openai", "a", {"key"},
                        {QVariantMap{{"kind", "session"},
                                     {"remainingPercent", 50},
                                     {"resetAt", now.addSecs(3600).toString(Qt::ISODate)}}},
                        now);
        QCOMPARE(points(history).first().toList()[2].toLongLong(),
                 now.addSecs(3600).toSecsSinceEpoch());
    }

    void rejectsUnsafeFilesystemEntries_data()
    {
        QTest::addColumn<QString>("kind");
        for (const auto &kind :
             {"directory-link", "directory-permissions", "directory-file", "file-link",
              "file-permissions", "file-hardlink", "fifo", "oversize", "locked"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }

    void rejectsUnsafeFilesystemEntries()
    {
        QFETCH(QString, kind);
        QTemporaryDir dir;
        const QString path = dir.filePath("history");
        const QString file = path + "/history.json";
        QVERIFY(QDir().mkdir(path));
        QVERIFY(
            QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        int lock = -1;
        if (kind == "directory-link") {
            QVERIFY(QDir().rmdir(path));
            QVERIFY(QFile::link(dir.path(), path));
        }
        else if (kind == "directory-permissions") {
            QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner |
                                                    QFile::ExeOwner | QFile::ReadGroup));
        }
        else if (kind == "directory-file") {
            QVERIFY(QDir().rmdir(path));
            writeFile(path, "untouched");
        }
        else if (kind == "file-link") {
            writeFile(dir.filePath("target"), "untouched");
            QVERIFY(QFile::link(dir.filePath("target"), file));
        }
        else if (kind == "file-hardlink") {
            writeFile(dir.filePath("target"), "untouched");
            QCOMPARE(::link(QFile::encodeName(dir.filePath("target")).constData(),
                            QFile::encodeName(file).constData()),
                     0);
        }
        else if (kind == "file-permissions") {
            writeFile(file, "{}");
            QVERIFY(QFile::setPermissions(file,
                                          QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup));
        }
        else if (kind == "fifo") {
            QCOMPARE(::mkfifo(QFile::encodeName(file).constData(), 0600), 0);
        }
        else if (kind == "oversize") {
            writeFile(file, QByteArray(8 * 1024 * 1024 + 1, ' '));
        }
        else if (kind == "locked") {
            lock = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_DIRECTORY);
            QVERIFY(lock >= 0);
            QCOMPARE(::flock(lock, LOCK_EX | LOCK_NB), 0);
        }
        QuotaHistory history;
        history.setDirectory(path);
        history.setPersistent(true);
        history.observe("openai", "a", {"key"}, quota());
        QVERIFY(!history.error().isEmpty());
        QVERIFY(history.view("openai").isEmpty());
        if (kind.startsWith("directory") || kind == "locked")
            QVERIFY(!history.clear());
        if (lock >= 0)
            ::close(lock);
        if (QFile::exists(dir.filePath("target"))) {
            QFile target(dir.filePath("target"));
            QVERIFY(target.open(QIODevice::ReadOnly));
            QCOMPARE(target.readAll(), QByteArray("untouched"));
        }
    }

    void rejectsMalformedFiles_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        const double now = static_cast<double>(QDateTime::currentSecsSinceEpoch());
        const QString key(64, 'a');
        const auto document = [&](const QJsonValue &streams) {
            return QJsonDocument(QJsonObject{{"version", 1}, {"streams", streams}}).toJson();
        };
        QTest::newRow("invalid-json") << QByteArray("bad");
        QTest::newRow("array") << QByteArray("[]");
        QTest::newRow("version") << QByteArray("{\"version\":2,\"streams\":{}}");
        QTest::newRow("fractional-version") << QByteArray("{\"version\":1.1,\"streams\":{}}");
        QTest::newRow("extra-field") << QByteArray("{\"version\":1,\"streams\":{},\"extra\":1}");
        QTest::newRow("wrong-streams") << document(1);
        QTest::newRow("bad-key") << document(QJsonObject{{"private", QJsonObject{}}});
        QTest::newRow("key-newline")
            << document(QJsonObject{{key + QLatin1Char('\n'), QJsonObject{}}});
        QTest::newRow("kind-newline")
            << document(QJsonObject{{key, QJsonObject{{"session\n", QJsonArray{}}}}});
        QTest::newRow("billing-window")
            << document(QJsonObject{{key, QJsonObject{{"spend-limit", QJsonArray{}}}}});
        QTest::newRow("bad-series") << document(QJsonObject{{key, 1}});
        QTest::newRow("bad-kind") << document(
            QJsonObject{{key, QJsonObject{{"../secret", QJsonArray{}}}}});
        QTest::newRow("not-array") << document(QJsonObject{{key, QJsonObject{{"session", 1}}}});
        const QList<QJsonArray> invalidRows{
            {now},         {"time", 50, 0},    {now, "50", 0},     {now, 50, "reset"},
            {now, -1, 0},  {now, 101, 0},      {now + 600, 50, 0}, {0, 50, 0},
            {now, 50, -1}, {now + 0.5, 50, 0}, {now, 50, 1.5}};
        for (qsizetype i = 0; i < invalidRows.size(); ++i) {
            QTest::newRow(qPrintable(QString::number(i))) << document(
                QJsonObject{{key, QJsonObject{{"session", QJsonArray{invalidRows[i]}}}}});
        }
        QTest::newRow("duplicates") << document(QJsonObject{
            {key, QJsonObject{
                      {"session", QJsonArray{QJsonArray{now, 50, 0}, QJsonArray{now, 50, 0}}}}}});
        QJsonObject streams;
        for (int i = 0; i < 65; ++i)
            streams.insert(QString::number(i), QJsonObject{});
        QTest::newRow("too-many-streams") << document(streams);
        QJsonObject series;
        for (int i = 0; i < 33; ++i)
            series.insert(QString::number(i), QJsonArray{});
        QTest::newRow("too-many-windows") << document(QJsonObject{{key, series}});
    }

    void rejectsMalformedFiles()
    {
        QFETCH(QByteArray, bytes);
        QTemporaryDir dir;
        QVERIFY(QFile::setPermissions(dir.path(),
                                      QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        writeFile(dir.filePath("history.json"), bytes);
        QuotaHistory history;
        history.setDirectory(dir.path());
        history.setPersistent(true);
        history.observe("openai", "a", {"key"}, quota());
        QVERIFY(!history.error().isEmpty());
        QVERIFY(history.view("openai").isEmpty());
        QVERIFY(history.clear());
        history.observe("openai", "a", {"key"}, quota());
        QCOMPARE(points(history).size(), 1);
    }

    void persistsOnlyValidatedQuotaAndSeparatesAccounts()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QStringLiteral("history"));
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const QVariantList windows{QVariantMap{{"kind", "session"}, {"remainingPercent", 65.0}}};
        {
            QuotaHistory history;
            history.setDirectory(path);
            history.setPersistent(true);
            history.observe("openai", "profile-a", {"identity-a"}, windows, now);
            QCOMPARE(history.view("openai").size(), 1);
            QCOMPARE(history.view("openai").first().toMap().value("points").toList().size(), 1);
            history.forgetProvider("openai");
            QVERIFY(history.view("openai").isEmpty());
            history.observe("openai", "profile-b", {"identity-b"}, windows, now);
            QCOMPARE(history.view("openai").first().toMap().value("points").toList().size(), 1);
        }
        QuotaHistory restored;
        restored.setDirectory(path);
        restored.setPersistent(true);
        QVERIFY(restored.view("openai").isEmpty());
        restored.observe("openai", "profile-a", {"identity-a"}, windows, now.addSecs(600));
        QCOMPARE(restored.view("openai").first().toMap().value("points").toList().size(), 2);
        QFile file(path + "/history.json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        QVERIFY(!bytes.contains("identity-a"));
        QVERIFY(!bytes.contains("profile-a"));
        QVERIFY(restored.clear());
        QVERIFY(restored.view("openai").isEmpty());
    }
};

QTEST_GUILESS_MAIN(QuotaHistoryTest)
#include "tst_quota_history.moc"
