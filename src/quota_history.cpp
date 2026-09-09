#include <kodometer/quota_history.hpp>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <fcntl.h>
#include <limits>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Kodometer {
namespace {
constexpr qint64 MaxBytes = 8 * 1024 * 1024;
constexpr qint64 RetentionSeconds = 30 * 86400;
constexpr qsizetype MaxPoints = 64000;

class Directory
{
  public:
    explicit Directory(const QString &path)
    {
        const QByteArray name = QFile::encodeName(path);
        if (::mkdir(name.constData(), 0700) != 0 && errno != EEXIST)
            return;
        fd = ::open(name.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        struct stat info{};
        if (fd >= 0 && (::fstat(fd, &info) != 0 || info.st_uid != ::getuid() ||
                        (info.st_mode & 0077) != 0 || ::flock(fd, LOCK_EX | LOCK_NB) != 0)) {
            ::close(fd);
            fd = -1;
        }
    }
    ~Directory()
    {
        release();
    }
    void release()
    {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }
    int fd = -1;
};

bool safeFile(int fd)
{
    struct stat info{};
    return ::fstat(fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_uid == ::getuid() &&
           (info.st_mode & 0077) == 0 && info.st_nlink == 1 && info.st_size <= MaxBytes;
}

QString streamKey(const QString &provider, const QByteArray &scope, const QByteArray &identity)
{
    QByteArray input = provider.toUtf8();
    input.append('\0');
    input.append(scope);
    input.append('\0');
    input.append(identity);
    const QByteArray digest = QCryptographicHash::hash(input, QCryptographicHash::Sha256);
    const QByteArray hex = digest.toHex();
    return QString::fromLatin1(hex);
}

bool supported(const QString &provider)
{
    static const QStringList providers{QStringLiteral("codex"), QStringLiteral("claude"),
                                       QStringLiteral("gemini"), QStringLiteral("kimi"),
                                       QStringLiteral("zai")};
    return providers.contains(provider);
}

bool validKind(const QString &kind)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9-]{0,127}\\z"));
    const auto match = pattern.match(kind);
    return match.hasMatch() && kind != QLatin1String("spend-limit");
}

double pointTime(const QJsonArray &points, qsizetype index)
{
    const QJsonValue value = points.at(index);
    const QJsonArray row = value.toArray();
    const QJsonValue time = row.at(0);
    return time.toDouble();
}
void mergeSeries(QJsonObject &destination, const QJsonObject &source)
{
    if (destination.isEmpty()) {
        destination = source;
        return;
    }
    for (auto window = source.begin(); window != source.end(); ++window) {
        const QJsonValue existingValue = destination.value(window.key());
        const QJsonValue incomingValue = window.value();
        QMap<qint64, QJsonArray> observations;
        const QList<QJsonArray> inputs{existingValue.toArray(), incomingValue.toArray()};
        for (const auto &input : inputs) {
            for (const auto &value : input) {
                const QJsonArray row = value.toArray();
                const QJsonValue timeValue = row.at(0);
                const double time = timeValue.toDouble();
                const qint64 bucket = static_cast<qint64>(time) / 300;
                const auto found = observations.constFind(bucket);
                if (found == observations.cend() || found->at(0).toDouble() <= time)
                    observations.insert(bucket, row);
            }
        }
        QJsonArray merged;
        for (const auto &row : std::as_const(observations))
            merged.append(row);
        destination.insert(window.key(), merged);
    }
}
} // namespace

QuotaHistory::QuotaHistory(QObject *parent) : QObject(parent) {}
bool QuotaHistory::persistent() const
{
    return m_persistent;
}
QString QuotaHistory::error() const
{
    return m_error;
}
int QuotaHistory::revision() const
{
    return m_revision;
}

void QuotaHistory::publish()
{
    ++m_revision;
    emit changed();
}

void QuotaHistory::setDirectory(const QString &directory)
{
    m_directory = directory;
    m_streams = {};
    m_selected.clear();
    publish();
}

void QuotaHistory::setPersistent(bool persistent)
{
    if (m_persistent == persistent)
        return;
    m_persistent = persistent;
    if (persistent && m_directory.isEmpty()) {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (QDir().mkpath(base))
            m_directory = QDir(base).filePath(QStringLiteral("kodometer-quota-history"));
    }
    publish();
}

bool QuotaHistory::load(int directory, const QDateTime &now)
{
    const int fd =
        ::openat(directory, "history.json", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) {
        if (errno == ENOENT) {
            m_streams = {};
            return true;
        }
        return false;
    }
    QFile file;
    if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return false;
    }
    if (!safeFile(fd))
        return false;
    const QByteArray bytes = file.read(MaxBytes + 1);
    if (bytes.size() > MaxBytes || file.error() != QFileDevice::NoError)
        return false;
    const auto document = QJsonDocument::fromJson(bytes);
    const auto root = document.object();
    const auto versionValue = root.value(QStringLiteral("version"));
    const auto streamsValue = root.value(QStringLiteral("streams"));
    if (!document.isObject() || root.size() != 2 || versionValue.toDouble() != 1 ||
        !streamsValue.isObject())
        return false;
    const auto streams = streamsValue.toObject();
    if (streams.size() > 64)
        return false;
    QJsonObject retained;
    qsizetype count = 0;
    const qint64 end = now.toSecsSinceEpoch();
    static const QRegularExpression hashPattern(QStringLiteral("^[a-f0-9]{64}\\z"));
    for (auto stream = streams.begin(); stream != streams.end(); ++stream) {
        const auto keyMatch = hashPattern.match(stream.key());
        const QJsonValue streamValue = stream.value();
        if (!keyMatch.hasMatch() || !streamValue.isObject())
            return false;
        const auto series = streamValue.toObject();
        if (series.size() > 32)
            return false;
        QJsonObject keptSeries;
        for (auto window = series.begin(); window != series.end(); ++window) {
            const QJsonValue windowValue = window.value();
            if (!validKind(window.key()) || !windowValue.isArray())
                return false;
            const auto points = windowValue.toArray();
            QJsonArray kept;
            double previous = 0;
            for (const auto &point : points) {
                const auto row = point.toArray();
                const QJsonValue timeValue = row.at(0);
                const QJsonValue remainingValue = row.at(1);
                const QJsonValue resetValue = row.at(2);
                if (++count > MaxPoints || row.size() != 3 || !timeValue.isDouble() ||
                    !remainingValue.isDouble() || !resetValue.isDouble())
                    return false;
                const double time = timeValue.toDouble();
                const double remaining = remainingValue.toDouble();
                const double reset = resetValue.toDouble();
                // QJsonValue's numeric form is finite; timestamps must also be whole seconds.
                if (std::floor(time) != time || time <= previous ||
                    time > static_cast<double>(end) + 300 || remaining < 0 || remaining > 100 ||
                    std::floor(reset) != reset || reset < 0)
                    return false;
                previous = time;
                if (time >= static_cast<double>(end - RetentionSeconds))
                    kept.append(row);
            }
            if (!kept.isEmpty())
                keptSeries.insert(window.key(), kept);
        }
        if (!keptSeries.isEmpty())
            retained.insert(stream.key(), keptSeries);
    }
    m_streams = retained;
    return true;
}

bool QuotaHistory::save(int directory)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("streams"), m_streams);
    const QJsonDocument document(root);
    const QByteArray bytes = document.toJson(QJsonDocument::Compact);
    if (bytes.size() > MaxBytes)
        return false;
    const QUuid uuid = QUuid::createUuid();
    QByteArray temporary("history-");
    temporary.append(uuid.toByteArray(QUuid::Id128));
    const int fd = ::openat(directory, temporary.constData(),
                            O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0)
        return false;
    QFile file;
    if (!file.open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        ::unlinkat(directory, temporary.constData(), 0);
        return false;
    }
    const bool written = file.write(bytes) == bytes.size() && file.flush() && ::fsync(fd) == 0;
    file.close();
    const bool replaced =
        written && ::renameat(directory, temporary.constData(), directory, "history.json") == 0;
    if (!replaced)
        ::unlinkat(directory, temporary.constData(), 0);
    return replaced;
}

void QuotaHistory::observe(const QString &provider, const QByteArray &scope,
                           const QList<QByteArray> &identities, const QVariantList &windows,
                           const QDateTime &now)
{
    prepareContext(provider, scope, identities);
    if (!supported(provider) || identities.isEmpty() || identities.last().isEmpty() ||
        !now.isValid() || windows.size() > 32)
        return;
    QJsonObject valid;
    const qint64 seconds = now.toSecsSinceEpoch();
    if (seconds <= 0)
        return;
    for (const auto &value : windows) {
        const auto window = value.toMap();
        const auto remaining = window.value(QStringLiteral("remainingPercent"));
        const auto kindValue = window.value(QStringLiteral("kind"));
        const QString kind = kindValue.toString();
        const auto idleValue = window.value(QStringLiteral("idle"));
        if (kindValue.metaType().id() != QMetaType::QString || !validKind(kind) ||
            idleValue.toBool() || remaining.isNull() ||
            (remaining.metaType().id() != QMetaType::Double &&
             remaining.metaType().id() != QMetaType::Int) ||
            !std::isfinite(remaining.toDouble()) || remaining.toDouble() < 0 ||
            remaining.toDouble() > 100)
            continue;
        if (valid.contains(kind))
            return; // Conflicting duplicate quota windows cannot form a reliable observation.
        const auto resetValue = window.value(QStringLiteral("resetAt"));
        const QString resetText = resetValue.toString();
        const auto reset = QDateTime::fromString(resetText, Qt::ISODate);
        const double resetSeconds =
            reset.isValid() ? static_cast<double>(qMax(qint64{0}, reset.toSecsSinceEpoch())) : 0.0;
        QJsonArray row;
        row.append(static_cast<double>(seconds));
        row.append(remaining.toDouble());
        row.append(resetSeconds);
        valid.insert(kind, row);
    }
    if (valid.isEmpty())
        return;
    Directory directory(m_persistent ? m_directory : QString{});
    m_error.clear();
    if (m_persistent && (directory.fd < 0 || !load(directory.fd, now))) {
        m_error = tr("Quota history could not be read safely. Usage is unaffected.");
        m_selected.remove(provider);
        directory.release();
        publish();
        return;
    }
    const QString key = streamKey(provider, scope, identities.last());
    QJsonObject series;
    for (const auto &identity : identities) {
        const QString alias = streamKey(provider, scope, identity);
        const QJsonValue aliasValue = m_streams.value(alias);
        const QJsonObject aliasSeries = aliasValue.toObject();
        mergeSeries(series, aliasSeries);
        if (alias != key)
            m_streams.remove(alias);
    }
    for (auto window = valid.begin(); window != valid.end(); ++window) {
        const QJsonValue windowValue = series.value(window.key());
        QJsonArray points = windowValue.toArray();
        if (!points.isEmpty() &&
            pointTime(points, points.size() - 1) > static_cast<double>(seconds))
            continue;
        while (!points.isEmpty() &&
               pointTime(points, 0) < static_cast<double>(seconds - RetentionSeconds))
            points.removeFirst();
        // Keep the latest actual observation per five-minute bucket, never invent an average.
        if (!points.isEmpty() &&
            static_cast<qint64>(pointTime(points, points.size() - 1)) / 300 == seconds / 300)
            points.removeLast();
        points.append(window.value());
        series.insert(window.key(), points);
    }
    while (series.size() > 32) {
        QString oldestKind;
        double oldestTime = std::numeric_limits<double>::max();
        for (auto window = series.begin(); window != series.end(); ++window) {
            const QJsonValue value = window.value();
            const QJsonArray points = value.toArray();
            const double last = pointTime(points, points.size() - 1);
            if (last < oldestTime) {
                oldestTime = last;
                oldestKind = window.key();
            }
        }
        series.remove(oldestKind);
    }
    m_streams.insert(key, series);
    // Hard global bounds: evict the oldest stream when storage is full.
    for (;;) {
        qsizetype count = 0;
        double oldest = std::numeric_limits<double>::max();
        QString oldestKey;
        for (auto stream = m_streams.begin(); stream != m_streams.end(); ++stream) {
            const QJsonValue streamValue = stream.value();
            const auto items = streamValue.toObject();
            double latest = 0;
            for (auto item = items.begin(); item != items.end(); ++item) {
                const QJsonValue itemValue = item.value();
                const auto points = itemValue.toArray();
                count += points.size();
                latest = std::max(latest, pointTime(points, points.size() - 1));
            }
            if (latest < oldest) {
                oldest = latest;
                oldestKey = stream.key();
            }
        }
        if (count <= MaxPoints && m_streams.size() <= 64)
            break;
        m_streams.remove(oldestKey);
    }
    m_selected.insert(provider, key);
    if (m_persistent && !save(directory.fd))
        m_error = tr("Quota history could not be saved. Usage is unaffected.");
    // Observers may clear the history; do not hold its file lock across publication.
    directory.release();
    publish();
}

QVariantList QuotaHistory::view(const QString &provider) const
{
    QVariantList result;
    const QString key = m_selected.value(provider);
    const auto seriesValue = m_streams.value(key);
    const auto series = seriesValue.toObject();
    for (auto window = series.begin(); window != series.end(); ++window) {
        const QJsonValue pointsValue = window.value();
        const auto points = pointsValue.toArray();
        QVariantMap row;
        row.insert(QStringLiteral("kind"), window.key());
        row.insert(QStringLiteral("points"), points.toVariantList());
        result.append(row);
    }
    return result;
}

void QuotaHistory::prepareContext(const QString &provider, const QByteArray &scope,
                                  const QList<QByteArray> &identities)
{
    const QString key =
        identities.isEmpty() ? QString{} : streamKey(provider, scope, identities.last());
    if (m_selected.value(provider) != key)
        forgetProvider(provider);
}

void QuotaHistory::forgetProvider(const QString &provider)
{
    if (m_selected.remove(provider) > 0)
        // Allow the controller to finish clearing all contexts before observers run.
        QMetaObject::invokeMethod(this, &QuotaHistory::publish, Qt::QueuedConnection);
}

bool QuotaHistory::clear()
{
    m_error.clear();
    if (m_persistent) {
        Directory directory(m_directory);
        if (directory.fd < 0 ||
            (::unlinkat(directory.fd, "history.json", 0) != 0 && errno != ENOENT)) {
            m_error = tr("Quota history could not be cleared.");
            directory.release();
            publish();
            return false;
        }
    }
    m_streams = {};
    m_selected.clear();
    publish();
    return true;
}
} // namespace Kodometer
