#pragma once

#include <kodometer/provider_adapter.hpp>
#include <kodometer/xai_credentials.hpp>
#include <kodometer/xai_usage_parser.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QMap>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;

namespace Kodometer {

class XaiProviderAdapter final : public ProviderAdapter
{
    Q_OBJECT

  public:
    static constexpr qsizetype MaximumResponseSize = 1024 * 1024;

    explicit XaiProviderAdapter(QNetworkAccessManager *network = nullptr,
                                QObject *parent = nullptr);

    [[nodiscard]] QString providerId() const override;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QVariantMap provider() const;

    void setBaseEndpoint(const QUrl &endpoint);
    void setEnvironment(const QMap<QString, QString> &environment);
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
        Balance,
        Usage
    };

    [[nodiscard]] QUrl endpoint(const QString &suffix) const;
    [[nodiscard]] QDateTime currentDateTime() const;
    void requestBalance();
    void requestUsage();
    void watchReply(QNetworkReply *reply, RequestKind kind);
    void readReplyData();
    void finishReply();
    void finishBalance(int statusCode, QNetworkReply::NetworkError networkError);
    void finishUsage(int statusCode);
    void completeSuccess(const std::optional<XaiUsageHistory> &history);
    void completeFailure(const QString &message);
    void setBusy(bool busy);
    void setError(const QString &error);

    QNetworkAccessManager *m_network = nullptr;
    QUrl m_baseEndpoint{QStringLiteral("https://management-api.x.ai")};
    QMap<QString, QString> m_environment;
    QDateTime m_currentDateTime;
    int m_timeoutMilliseconds = 30'000;
    XaiCredentials m_credentials;
    XaiBalance m_balance;
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
