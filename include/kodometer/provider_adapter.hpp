#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace Kodometer {

class ProviderAdapter : public QObject
{
    Q_OBJECT

  public:
    using QObject::QObject;
    ~ProviderAdapter() override = default;

    [[nodiscard]] virtual QString providerId() const = 0;
    virtual void refresh() = 0;

  signals:
    void refreshSucceeded(const QVariantMap &provider);
    void refreshFailed(const QString &error);
};

} // namespace Kodometer
