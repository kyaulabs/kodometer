#pragma once

#include <QObject>
#include <QProcess>
#include <QQmlEngine>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace CodexBar {

class DashboardController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString executable READ executable WRITE setExecutable NOTIFY executableChanged)
    Q_PROPERTY(int timeoutMilliseconds READ timeoutMilliseconds WRITE setTimeoutMilliseconds NOTIFY
                   timeoutMillisecondsChanged)
    Q_PROPERTY(bool identityRedacted READ identityRedacted WRITE setIdentityRedacted NOTIFY
                   identityRedactedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList providers READ providers NOTIFY snapshotChanged)

  public:
    explicit DashboardController(QObject *parent = nullptr);

    [[nodiscard]] QString executable() const;
    void setExecutable(const QString &executable);

    [[nodiscard]] int timeoutMilliseconds() const noexcept;
    void setTimeoutMilliseconds(int timeoutMilliseconds);

    [[nodiscard]] bool identityRedacted() const noexcept;
    void setIdentityRedacted(bool identityRedacted);

    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QVariantMap snapshot() const;
    [[nodiscard]] QVariantList providers() const;

    Q_INVOKABLE void refresh();

  signals:
    void executableChanged();
    void timeoutMillisecondsChanged();
    void identityRedactedChanged();
    void busyChanged();
    void errorChanged();
    void snapshotChanged();
    void refreshFinished(bool success);

  private:
    void readStandardOutput();
    void finishProcess(int exitCode, QProcess::ExitStatus exitStatus);
    void processError(QProcess::ProcessError processError);
    void processTimedOut();
    void completeFailure(const QString &message);
    void setBusy(bool busy);
    void setError(const QString &error);

    QString m_executable = QStringLiteral("codexbar");
    int m_timeoutMilliseconds = 30'000;
    bool m_identityRedacted = true;
    bool m_busy = false;
    bool m_outputLimitExceeded = false;
    bool m_timedOut = false;
    QString m_error;
    QVariantMap m_snapshot;
    QProcess m_process;
    QTimer m_timeout;
    QByteArray m_standardOutput;
};

} // namespace CodexBar
