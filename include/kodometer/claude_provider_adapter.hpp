#pragma once

#include <kodometer/claude_credentials.hpp>
#include <kodometer/provider_adapter.hpp>

#include <QByteArray>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;

namespace Kodometer {

class ClaudeProviderAdapter final : public ProviderAdapter
{
    Q_OBJECT

  public:
    static constexpr qsizetype MaximumResponseSize = 1024 * 1024;

    explicit ClaudeProviderAdapter(QNetworkAccessManager *network = nullptr,
                                   QObject *parent = nullptr);

    [[nodiscard]] QString providerId() const override;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QVariantMap provider() const;

    void setCredentialPath(const QString &path);
    void setProfileDirectory(const QString &directory) override;
    void setUsageEndpoint(const QUrl &endpoint);
    void setTokenEndpoint(const QUrl &endpoint);
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
        Usage,
        Token
    };

    void requestUsage();
    void requestToken();
    void watchReply(QNetworkReply *reply, RequestKind kind);
    void readReplyData();
    void finishReply();
    void finishUsage(int statusCode, QNetworkReply::NetworkError networkError);
    void finishToken(int statusCode, QNetworkReply::NetworkError networkError);
    void completeSuccess(const QVariantMap &provider);
    void completeFailure(const QString &message);
    void setBusy(bool busy);
    void setError(const QString &error);

    QNetworkAccessManager *m_network = nullptr;
    QString m_credentialPath;
    QString m_defaultCredentialPath;
    QString m_requestCredentialPath;
    QUrl m_usageEndpoint{QStringLiteral("https://api.anthropic.com/api/oauth/usage")};
    QUrl m_tokenEndpoint{QStringLiteral("https://platform.claude.com/v1/oauth/token")};
    int m_timeoutMilliseconds = 30'000;
    ClaudeCredentials m_credentials;
    QVariantMap m_provider;
    QString m_error;
    QByteArray m_responseData;
    QNetworkReply *m_reply = nullptr;
    RequestKind m_requestKind = RequestKind::None;
    QTimer m_timeout;
    bool m_busy = false;
    bool m_timedOut = false;
    bool m_responseTooLarge = false;
    bool m_refreshedDuringRequest = false;
};

} // namespace Kodometer
