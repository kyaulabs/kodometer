#pragma once

#include <kodometer/gemini_credentials.hpp>
#include <kodometer/gemini_usage_parser.hpp>
#include <kodometer/provider_adapter.hpp>

#include <QByteArray>
#include <QMap>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;

namespace Kodometer {

class GeminiProviderAdapter final : public ProviderAdapter
{
    Q_OBJECT

  public:
    static constexpr qsizetype MaximumResponseSize = 1024 * 1024;

    explicit GeminiProviderAdapter(QNetworkAccessManager *network = nullptr,
                                   QObject *parent = nullptr);

    [[nodiscard]] QString providerId() const override;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QVariantMap provider() const;

    void setCredentialPath(const QString &path);
    void setSettingsPath(const QString &path);
    void setTokenEndpoint(const QUrl &endpoint);
    void setCodeAssistEndpoint(const QUrl &endpoint);
    void setProjectsEndpoint(const QUrl &endpoint);
    void setQuotaEndpoint(const QUrl &endpoint);
    void setOAuthEnvironment(const QMap<QString, QString> &environment);
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
        Token,
        CodeAssist,
        Projects,
        Quota
    };

    void requestToken();
    void requestCodeAssist();
    void requestProjects();
    void requestQuota();
    void watchReply(QNetworkReply *reply, RequestKind kind);
    void readReplyData();
    void finishReply();
    void finishToken(int statusCode, QNetworkReply::NetworkError networkError);
    void finishCodeAssist(int statusCode);
    void finishProjects(int statusCode);
    void finishQuota(int statusCode, QNetworkReply::NetworkError networkError);
    void continueAfterOptionalFailure(RequestKind kind);
    void completeSuccess(const QVariantMap &provider);
    void completeFailure(const QString &message);
    void setBusy(bool busy);
    void setError(const QString &error);

    QNetworkAccessManager *m_network = nullptr;
    QString m_credentialPath;
    QString m_settingsPath;
    QUrl m_tokenEndpoint{QStringLiteral("https://oauth2.googleapis.com/token")};
    QUrl m_codeAssistEndpoint{
        QStringLiteral("https://cloudcode-pa.googleapis.com/v1internal:loadCodeAssist")};
    QUrl m_projectsEndpoint{
        QStringLiteral("https://cloudresourcemanager.googleapis.com/v1/projects")};
    QUrl m_quotaEndpoint{
        QStringLiteral("https://cloudcode-pa.googleapis.com/v1internal:retrieveUserQuota")};
    QMap<QString, QString> m_oauthEnvironment;
    int m_timeoutMilliseconds = 30'000;
    GeminiCredentials m_credentials;
    GeminiCodeAssistStatus m_codeAssistStatus;
    QVariantMap m_provider;
    QString m_projectId;
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
