#pragma once

#include <QMap>
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

    void setCredentialOverrides(const QMap<QString, QString> &overrides);

  signals:
    void refreshSucceeded(const QVariantMap &provider);
    void refreshFailed(const QString &error);

  protected:
    [[nodiscard]] QMap<QString, QString>
    environmentWithCredentialOverrides(const QMap<QString, QString> &environment) const;

  private:
    QMap<QString, QString> m_credentialOverrides;
};

} // namespace Kodometer
