#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QThread>

#include <cstdlib>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QByteArray mode = qgetenv("CODEXBAR_TEST_MODE");

    if (mode == "success") {
        QFile fixture(QStringLiteral(FIXTURE_PATH));
        if (!fixture.open(QIODevice::ReadOnly)) {
            return 99;
        }
        fwrite(fixture.readAll().constData(), 1, static_cast<size_t>(fixture.size()), stdout);
        return 0;
    }
    if (mode == "malformed") {
        QTextStream(stdout) << "{";
        return 0;
    }
    if (mode == "oversized") {
        const QByteArray output(4 * 1024 * 1024 + 1, 'x');
        fwrite(output.constData(), 1, static_cast<size_t>(output.size()), stdout);
        return 0;
    }
    if (mode == "slow") {
        QThread::msleep(500);
        return 0;
    }
    if (mode == "silent-failure") {
        return 7;
    }
    if (mode == "crash") {
        std::abort();
    }

    QTextStream(stderr) << "provider request failed\n";
    return 7;
}
