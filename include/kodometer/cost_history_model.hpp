#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

namespace Kodometer {

class CostHistoryModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap cost READ cost WRITE setCost NOTIFY costChanged)
    Q_PROPERTY(int days READ days WRITE setDays NOTIFY daysChanged)
    Q_PROPERTY(QVariantMap view READ view NOTIFY viewChanged)

  public:
    explicit CostHistoryModel(QObject *parent = nullptr);
    [[nodiscard]] QVariantMap cost() const;
    [[nodiscard]] int days() const noexcept;
    [[nodiscard]] QVariantMap view() const;
    void setCost(const QVariantMap &cost);
    void setDays(int days);

  signals:
    void costChanged();
    void daysChanged();
    void viewChanged();

  private:
    [[nodiscard]] static QVariantMap buildView(const QVariantMap &cost, int days);
    void setView(const QVariantMap &view);

    QVariantMap m_cost;
    QVariantMap m_view;
    int m_days = 30;
};

} // namespace Kodometer
