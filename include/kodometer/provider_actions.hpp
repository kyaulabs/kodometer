#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantList>

#include <functional>

namespace Kodometer {

class ProviderActions : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString providerId READ providerId WRITE setProviderId NOTIFY contextChanged)
    Q_PROPERTY(QString region READ region WRITE setRegion NOTIFY contextChanged)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY contextChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  public:
    using UrlOpener = std::function<bool(const QUrl &)>;

    explicit ProviderActions(QObject *parent = nullptr);
    explicit ProviderActions(UrlOpener opener, QObject *parent = nullptr);

    [[nodiscard]] QString providerId() const;
    [[nodiscard]] QString region() const;
    [[nodiscard]] QVariantList actions() const;
    [[nodiscard]] QString error() const;
    void setProviderId(const QString &providerId);
    void setRegion(const QString &region);

    Q_INVOKABLE bool open(const QString &actionId);

  signals:
    void contextChanged();
    void errorChanged();

  private:
    void setError(const QString &error);

    UrlOpener m_opener;
    QString m_providerId;
    QString m_region;
    QString m_error;
};

} // namespace Kodometer
