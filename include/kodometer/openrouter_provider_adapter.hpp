#pragma once

#include <kodometer/openrouter_credentials.hpp>
#include <kodometer/openrouter_usage_parser.hpp>
#include <kodometer/provider_adapter.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QMap>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;

namespace Kodometer {

class OpenRouterProviderAdapter final : public ProviderAdapter
{
    Q_OBJECT

  public:
    static constexpr qsizetype MaximumResponseSize = 1024 * 1024;

    explicit OpenRouterProviderAdapter(QNetworkAccessManager *network = nullptr,
                                       QObject *parent = nullptr);

    [[nodiscard]] QString providerId() const override;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QVariantMap provider() const;

    void setApiBaseEndpoint(const QUrl &endpoint);
    void setActivityEndpoint(const QUrl &endpoint);
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
        Credits,
        Key,
        ActivityHistory,
        ActivityLatest
    };

    [[nodiscard]] QDateTime currentDateTime() const;
    [[nodiscard]] QUrl apiEndpoint(const QString &resource) const;
    [[nodiscard]] QNetworkRequest requestFor(const QUrl &url, const QString &token,
                                             bool applicationHeaders) const;
    void requestCredits();
    void requestKey();
    void requestActivityHistory();
    void requestActivityLatest();
    void watchReply(QNetworkReply *reply, RequestKind kind);
    void readReplyData();
    void finishReply();
    void finishCredits(int statusCode, QNetworkReply::NetworkError networkError);
    void finishKey(int statusCode, QNetworkReply::NetworkError networkError);
    void finishActivity(RequestKind kind, int statusCode, QNetworkReply::NetworkError networkError);
    void completeSuccess();
    void completeFailure(const QString &message);
    void setBusy(bool busy);
    void setError(const QString &error);
    [[nodiscard]] QString optionalDiagnostic(int statusCode,
                                             QNetworkReply::NetworkError networkError) const;

    QNetworkAccessManager *m_network = nullptr;
    QUrl m_apiBaseEndpoint{QStringLiteral("https://openrouter.ai/api/v1")};
    QUrl m_activityEndpoint{QStringLiteral("https://openrouter.ai/api/v1/activity")};
    QMap<QString, QString> m_environment;
    QDateTime m_currentDateTime;
    QDateTime m_refreshDateTime;
    int m_primaryTimeoutMilliseconds = 30'000;
    int m_optionalTimeoutMilliseconds = 1'000;
    OpenRouterCredentials m_credentials;
    OpenRouterCredits m_credits;
    std::optional<OpenRouterKeyUsage> m_keyUsage;
    std::optional<OpenRouterActivity> m_activityHistory;
    std::optional<OpenRouterActivity> m_activityLatest;
    QString m_keyDiagnostic;
    QString m_activityDiagnostic;
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
