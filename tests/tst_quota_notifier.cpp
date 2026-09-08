#include <kodometer/quota_notifier.hpp>

#include <QDBusConnection>
#include <QTemporaryDir>
#include <QtTest>

class NotificationService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
  public:
    QString title;
    QString body;
    QString icon;
    QStringList actions;
    QVariantMap hints;
    uint count = 0;

  public slots:
    QStringList GetCapabilities()
    {
        return {QStringLiteral("body"), QStringLiteral("body-markup")};
    }
    QString GetServerInformation(QString &vendor, QString &version, QString &specification)
    {
        vendor = QStringLiteral("Kodometer tests");
        version = QStringLiteral("1");
        specification = QStringLiteral("1.2");
        return QStringLiteral("Test notification service");
    }
    uint Notify(const QString &, uint, const QString &appIcon, const QString &summary,
                const QString &text, const QStringList &buttons, const QVariantMap &metadata, int)
    {
        title = summary;
        body = text;
        icon = appIcon;
        actions = buttons;
        hints = metadata;
        ++count;
        return count;
    }
    void CloseNotification(uint id)
    {
        emit NotificationClosed(id, 3);
    }

  signals:
    void NotificationClosed(uint id, uint reason);
};

class QuotaNotifierTest final : public QObject
{
    Q_OBJECT
  private slots:
    void deliversNormalPrivateAlertsThroughKDE()
    {
        QVERIFY(qEnvironmentVariableIsSet("KODOMETER_TEST_PRIVATE_BUS"));
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        // CTest supplies a private bus. Never register this fake on the user's desktop bus.
        QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.Notifications")));
        NotificationService service;
        QVERIFY(bus.registerObject(QStringLiteral("/org/freedesktop/Notifications"), &service,
                                   QDBusConnection::ExportAllSlots |
                                       QDBusConnection::ExportAllSignals));
        {
            Kodometer::QuotaNotifier notifier;
            QVariantMap provider{{QStringLiteral("id"), QStringLiteral("codex")},
                                 {QStringLiteral("name"), QStringLiteral("private@example.test")},
                                 {QStringLiteral("windows"),
                                  QVariantList{QVariantMap{{QStringLiteral("remainingPercent"), 5},
                                                           {QStringLiteral("label"),
                                                            QStringLiteral("private account")}}}}};
            notifier.observe(provider);
            QCOMPARE(service.count, 0U);
            notifier.setEnabled(true);
            notifier.observe(provider);
            QTRY_COMPARE(service.count, 1U);
            QCOMPARE(service.title, QStringLiteral("Codex quota is low"));
            QCOMPARE(service.body, QStringLiteral("Most constrained quota: 5.0% remaining."));
            QCOMPARE(service.icon, QStringLiteral("kodometer"));
            QCOMPARE(service.hints.value(QStringLiteral("urgency")).toUInt(), 1U);
            QVERIFY(service.actions.isEmpty());
            notifier.observe(provider);
            provider.insert(QStringLiteral("id"), QStringLiteral("claude"));
            notifier.observe(provider);
            QTRY_COMPARE(service.count, 2U);
            QCOMPARE(service.title, QStringLiteral("Claude quota is low"));
        }
        bus.unregisterObject(QStringLiteral("/org/freedesktop/Notifications"));
        bus.unregisterService(QStringLiteral("org.freedesktop.Notifications"));
    }

  private:
    QTemporaryDir m_config;
};

QTEST_MAIN(QuotaNotifierTest)
#include "tst_quota_notifier.moc"
