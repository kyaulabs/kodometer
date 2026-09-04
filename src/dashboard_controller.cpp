#include <kodometer/dashboard_controller.hpp>

#include <kodometer/dashboard_snapshot.hpp>

#include <QProcessEnvironment>

#include <algorithm>

namespace Kodometer {
namespace {

constexpr qsizetype MaximumDiagnosticBytes = 4 * 1024;

QString commandFailureMessage(const QByteArray &standardError, int exitCode)
{
    const QString diagnostic =
        QString::fromUtf8(standardError.left(MaximumDiagnosticBytes)).trimmed();
    if (!diagnostic.isEmpty()) {
        return diagnostic;
    }
    return QStringLiteral("CodexBar exited with code %1").arg(exitCode);
}

} // namespace

DashboardController::DashboardController(QObject *parent) : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_timeout.setSingleShot(true);

    connect(&m_process, &QProcess::readyReadStandardOutput, this,
            &DashboardController::readStandardOutput);
    connect(&m_process, &QProcess::finished, this, &DashboardController::finishProcess);
    connect(&m_process, &QProcess::errorOccurred, this, &DashboardController::processError);
    connect(&m_timeout, &QTimer::timeout, this, &DashboardController::processTimedOut);
}

QString DashboardController::executable() const
{
    return m_executable;
}

void DashboardController::setExecutable(const QString &executable)
{
    if (m_executable == executable) {
        return;
    }
    m_executable = executable;
    emit executableChanged();
}

int DashboardController::timeoutMilliseconds() const noexcept
{
    return m_timeoutMilliseconds;
}

void DashboardController::setTimeoutMilliseconds(int timeoutMilliseconds)
{
    const int boundedTimeout = std::clamp(timeoutMilliseconds, 1, 86'400'000);
    if (m_timeoutMilliseconds == boundedTimeout) {
        return;
    }
    m_timeoutMilliseconds = boundedTimeout;
    emit timeoutMillisecondsChanged();
}

bool DashboardController::identityRedacted() const noexcept
{
    return m_identityRedacted;
}

void DashboardController::setIdentityRedacted(bool identityRedacted)
{
    if (m_identityRedacted == identityRedacted) {
        return;
    }
    m_identityRedacted = identityRedacted;
    emit identityRedactedChanged();
}

bool DashboardController::busy() const noexcept
{
    return m_busy;
}

QString DashboardController::error() const
{
    return m_error;
}

QVariantMap DashboardController::snapshot() const
{
    return m_snapshot;
}

QVariantList DashboardController::providers() const
{
    return m_snapshot.value(QStringLiteral("providers")).toList();
}

void DashboardController::refresh()
{
    if (m_busy) {
        return;
    }

    m_standardOutput.clear();
    m_outputLimitExceeded = false;
    m_timedOut = false;
    setError({});
    setBusy(true);

    const int timeoutSeconds = std::max(1, (m_timeoutMilliseconds + 999) / 1'000);
    QStringList arguments;
    arguments.reserve(5);
    arguments.append(QStringLiteral("dashboard"));
    arguments.append(QStringLiteral("--identity"));
    arguments.append(m_identityRedacted ? QStringLiteral("redacted") : QStringLiteral("full"));
    arguments.append(QStringLiteral("--timeout"));
    arguments.append(QString::number(timeoutSeconds));

    m_process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    m_process.start(m_executable, arguments, QIODevice::ReadOnly);
    m_timeout.start(m_timeoutMilliseconds);
}

void DashboardController::readStandardOutput()
{
    const QByteArray output = m_process.readAllStandardOutput();
    if (m_standardOutput.size() + output.size() > DashboardSnapshot::MaximumPayloadBytes) {
        m_outputLimitExceeded = true;
        m_process.kill();
        return;
    }
    m_standardOutput.append(output);
}

void DashboardController::finishProcess(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_timeout.stop();
    readStandardOutput();

    if (m_outputLimitExceeded) {
        completeFailure(QStringLiteral("CodexBar dashboard output is too large"));
        return;
    }
    if (m_timedOut) {
        completeFailure(QStringLiteral("CodexBar dashboard request timed out"));
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        completeFailure(commandFailureMessage(m_process.readAllStandardError(), exitCode));
        return;
    }

    QString parseError;
    const auto parsed = DashboardSnapshot::fromJson(m_standardOutput, &parseError);
    if (!parsed.has_value()) {
        completeFailure(parseError);
        return;
    }

    m_snapshot = parsed->toVariantMap();
    emit snapshotChanged();
    setBusy(false);
    emit refreshFinished(true);
}

void DashboardController::processError(QProcess::ProcessError processError)
{
    if (processError != QProcess::FailedToStart) {
        return;
    }
    m_timeout.stop();
    completeFailure(QStringLiteral("Could not start CodexBar: %1").arg(m_process.errorString()));
}

void DashboardController::processTimedOut()
{
    m_timedOut = true;
    m_process.kill();
}

void DashboardController::completeFailure(const QString &message)
{
    setError(message);
    setBusy(false);
    emit refreshFinished(false);
}

void DashboardController::setBusy(bool busy)
{
    m_busy = busy;
    emit busyChanged();
}

void DashboardController::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

} // namespace Kodometer
