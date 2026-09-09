#pragma once

#include <kodometer/provider_adapter.hpp>
#include <kodometer/zai_credentials.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QMap>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkRequest;

namespace Kodometer {

class ZaiProviderAdapter final : public ProviderAdapter
{
    Q_OBJECT

  public:
    static constexpr qsizetype MaximumResponseSize = 1024 * 1024;

    explicit ZaiProviderAdapter(QNetworkAccessManager *network = nullptr,
                                QObject *parent = nullptr);

    [[nodiscard]] QString providerId() const override;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QVariantMap provider() const;

    void setEnvironment(const QMap<QString, QString> &environment);
    [[nodiscard]] QByteArray defaultCredentialContext() const override;
    void setHomeDirectory(const QString &homeDirectory);
    void setQuotaEndpoint(const QUrl &endpoint);
    void setModelUsageEndpoint(const QUrl &endpoint);
    void setBalanceEndpoint(const QUrl &endpoint);
    void setCurrentDateTime(const QDateTime &dateTime);
    void setTimeoutMilliseconds(int milliseconds);

    void refresh() override;

  signals:
    void busyChanged();
    void errorChanged();
    void providerChanged();
    void refreshFinished(bool success);

  private:
    enum class RequestKind
    {
        None,
        Quota,
        HourlyUsage,
        DailyUsage,
        Balance
    };

    [[nodiscard]] QDateTime currentDateTime() const;
    [[nodiscard]] QUrl productionQuotaEndpoint() const;
    [[nodiscard]] QUrl productionModelUsageEndpoint() const;
    [[nodiscard]] QUrl productionBalanceEndpoint() const;
    [[nodiscard]] QNetworkRequest requestFor(const QUrl &url, bool teamHeaders) const;
    [[nodiscard]] QUrl modelUsageUrl(int daysBack) const;
    void requestQuota();
    void requestModelUsage(RequestKind kind, int daysBack);
    void requestBalance();
    void watchReply(QNetworkReply *reply, RequestKind kind);
    void readReplyData();
    void finishReply();
    void advanceOptionalRequest(RequestKind completedKind);
    void completeSuccess();
    void completeFailure(const QString &message);
    void setBusy(bool busy);
    void setError(const QString &error);

    QNetworkAccessManager *m_network = nullptr;
    QMap<QString, QString> m_environment;
    QString m_homeDirectory;
    QUrl m_quotaEndpoint;
    QUrl m_modelUsageEndpoint;
    QUrl m_balanceEndpoint;
    QDateTime m_currentDateTime;
    int m_timeoutMilliseconds = 30'000;
    ZaiCredentials m_credentials;
    QVariantMap m_pendingProvider;
    QVariantMap m_provider;
    QString m_error;
    QByteArray m_responseData;
    QNetworkReply *m_reply = nullptr;
    RequestKind m_requestKind = RequestKind::None;
    QTimer m_timeout;
    bool m_busy = false;
    bool m_timedOut = false;
    bool m_responseTooLarge = false;
};

} // namespace Kodometer
