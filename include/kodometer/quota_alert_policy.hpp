#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QVariantMap>

namespace Kodometer {

class QuotaAlertPolicy : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(
        int thresholdPercent READ thresholdPercent WRITE setThresholdPercent NOTIFY settingsChanged)

  public:
    explicit QuotaAlertPolicy(QObject *parent = nullptr);
    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] int thresholdPercent() const noexcept;
    void setEnabled(bool enabled);
    void setThresholdPercent(int percent);
    Q_INVOKABLE void observe(const QVariantMap &provider);
    Q_INVOKABLE void forgetProvider(const QString &provider);

  signals:
    void settingsChanged();
    void alertReady(const QString &title, const QString &body);

  private:
    QSet<QString> m_lowProviders;
    int m_thresholdPercent = 10;
    bool m_enabled = false;
};

} // namespace Kodometer
