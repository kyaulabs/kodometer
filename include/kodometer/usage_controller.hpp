#pragma once

#include <kodometer/credential_store.hpp>
#include <kodometer/provider_adapter.hpp>

#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace Kodometer {

class UsageController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList providers READ providers NOTIFY providersChanged)
    Q_PROPERTY(bool autoRefresh READ autoRefresh WRITE setAutoRefresh NOTIFY refreshSettingsChanged)
    Q_PROPERTY(int refreshIntervalMinutes READ refreshIntervalMinutes WRITE
                   setRefreshIntervalMinutes NOTIFY refreshSettingsChanged)
    Q_PROPERTY(QStringList disabledProviders READ disabledProviders WRITE setDisabledProviders
                   NOTIFY disabledProvidersChanged)
    Q_PROPERTY(Kodometer::CredentialStore *credentialStore READ credentialStore WRITE
                   setCredentialStore NOTIFY credentialStoreChanged)

  public:
    explicit UsageController(QObject *parent = nullptr);
    explicit UsageController(const QList<ProviderAdapter *> &adapters, QObject *parent = nullptr);

    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QVariantMap snapshot() const;
    [[nodiscard]] QVariantList providers() const;
    [[nodiscard]] CredentialStore *credentialStore() const noexcept;

    [[nodiscard]] bool autoRefresh() const noexcept;
    [[nodiscard]] int refreshIntervalMinutes() const noexcept;
    [[nodiscard]] QStringList disabledProviders() const;
    void setAutoRefresh(bool enabled);
    void setRefreshIntervalMinutes(int minutes);
    void setDisabledProviders(const QStringList &providers);
    void setCredentialStore(CredentialStore *store);
    Q_INVOKABLE void refresh();

  signals:
    void busyChanged();
    void errorChanged();
    void snapshotChanged();
    void providersChanged();
    void credentialStoreChanged();
    void refreshFinished(bool success);
    void refreshSettingsChanged();
    void disabledProvidersChanged();

  private:
    void registerAdapter(ProviderAdapter *adapter);
    void adapterSucceeded(ProviderAdapter *adapter, const QVariantMap &provider);
    void adapterFailed(ProviderAdapter *adapter, const QString &error);
    void finishAdapter();
    void rebuildProviders();
    void rebuildErrors();
    void scheduleRefresh();
    [[nodiscard]] QList<ProviderAdapter *> enabledAdapters() const;
    void applyCredentialOverrides();
    void credentialsChanged();
    void setBusy(bool busy);
    void setError(const QString &error);

    QList<ProviderAdapter *> m_adapters;
    QMap<QString, QVariantMap> m_providerSnapshots;
    QVariantList m_providers;
    QVariantMap m_snapshot;
    QMap<QString, QString> m_refreshErrors;
    QStringList m_disabledProviders;
    QTimer m_refreshTimer;
    int m_refreshIntervalMinutes = 5;
    bool m_autoRefresh = true;
    bool m_started = false;
    QString m_error;
    CredentialStore *m_credentialStore = nullptr;
    QSet<ProviderAdapter *> m_pendingAdapters;
    bool m_busy = false;
    bool m_anySuccess = false;
    bool m_refreshAfterCurrent = false;
};

} // namespace Kodometer
