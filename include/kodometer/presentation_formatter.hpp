#pragma once

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariant>

namespace Kodometer {

class PresentationFormatter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

  public:
    using QObject::QObject;

    Q_INVOKABLE static double clampPercent(double percent) noexcept;
    Q_INVOKABLE static QString remainingLabel(const QVariant &remainingPercent);
    Q_INVOKABLE static QString resetLabel(const QString &timestamp);
    Q_INVOKABLE static QString windowTitle(const QString &kind, const QString &label);
    Q_INVOKABLE static QString creditsLabel(double amount, const QString &unit);
    Q_INVOKABLE static QString usdLabel(double amount);

    [[nodiscard]] static QString resetLabelAt(const QString &timestamp, const QDateTime &now);

  private:
    [[nodiscard]] static QString compactNumber(double amount);
};

} // namespace Kodometer
