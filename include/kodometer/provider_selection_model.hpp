#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace Kodometer {

class ProviderSelectionModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList providers READ providers WRITE setProviders NOTIFY providersChanged)
    Q_PROPERTY(QString selectedProviderId READ selectedProviderId NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedProvider READ selectedProvider NOTIFY selectionChanged)
    Q_PROPERTY(bool overviewSelected READ overviewSelected NOTIFY selectionChanged)
    Q_PROPERTY(int selectedTabIndex READ selectedTabIndex NOTIFY selectionChanged)

  public:
    explicit ProviderSelectionModel(QObject *parent = nullptr);

    [[nodiscard]] QVariantList providers() const;
    void setProviders(const QVariantList &providers);

    [[nodiscard]] QString selectedProviderId() const;
    [[nodiscard]] QVariantMap selectedProvider() const;
    [[nodiscard]] bool overviewSelected() const noexcept;
    [[nodiscard]] int selectedTabIndex() const;

    Q_INVOKABLE void selectOverview();
    Q_INVOKABLE void selectProvider(const QString &providerId);
    Q_INVOKABLE void selectNext();
    Q_INVOKABLE void selectPrevious();

  signals:
    void providersChanged();
    void selectionChanged();

  private:
    [[nodiscard]] qsizetype providerIndex(const QString &providerId) const;
    [[nodiscard]] QString providerIdAt(qsizetype index) const;
    void setSelection(const QString &providerId);
    void selectTab(qsizetype tabIndex);

    QVariantList m_providers;
    QString m_selectedProviderId;
};

} // namespace Kodometer
