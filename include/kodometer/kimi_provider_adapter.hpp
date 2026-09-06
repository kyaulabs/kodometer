#pragma once

#include <kodometer/kimi_credentials.hpp>
#include <kodometer/provider_adapter.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QMap>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;

namespace Kodometer {

class KimiProviderAdapter final : public ProviderAdapter
{
    Q_OBJECT

  public:
    static constexpr qsizetype MaximumResponseSize = 1024 * 1024;

    explicit KimiProviderAdapter(QNetworkAccessManager *network = nullptr,
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
    [[nodiscard]] QDateTime currentDateTime() const;
    [[nodiscard]] QUrl usageEndpoint() const;
    void requestUsage();
    [[nodiscard]] bool tryCliFallback();
    void readReplyData();
    void finishReply();
    void completeSuccess(const QVariantMap &provider);
    void completeFailure(const QString &message);
    void setBusy(bool busy);
    void setError(const QString &error);

    QNetworkAccessManager *m_network = nullptr;
    QUrl m_baseEndpoint{QStringLiteral("https://api.kimi.com")};
    QMap<QString, QString> m_environment;
    QDateTime m_currentDateTime;
    int m_timeoutMilliseconds = 30'000;
    KimiCredentials m_credentials;
    QVariantMap m_provider;
    QString m_error;
    QByteArray m_responseData;
    QNetworkReply *m_reply = nullptr;
    QTimer m_timeout;
    bool m_busy = false;
    bool m_timedOut = false;
    bool m_responseTooLarge = false;
    bool m_triedCliFallback = false;
    bool m_requestUsesNamedAccount = false;
};

} // namespace Kodometer
