#pragma once

#include <kodometer/provider_adapter.hpp>

#include <QByteArray>
#include <QDateTime>
#include <QMap>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;

namespace Kodometer {

class DeepSeekProviderAdapter final : public ProviderAdapter
{
    Q_OBJECT

  public:
    static constexpr qsizetype MaximumResponseSize = 1024 * 1024;

    explicit DeepSeekProviderAdapter(QNetworkAccessManager *network = nullptr,
                                     QObject *parent = nullptr);

    [[nodiscard]] QString providerId() const override;
    [[nodiscard]] bool busy() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QVariantMap provider() const;

    void setBaseEndpoint(const QUrl &endpoint);
    void setEnvironment(const QMap<QString, QString> &environment);
    [[nodiscard]] QByteArray defaultCredentialContext() const override;
    void setCurrentDateTime(const QDateTime &dateTime);
    void setTimeoutMilliseconds(int milliseconds);

    void refresh() override;

  signals:
    void busyChanged();
    void errorChanged();
    void providerChanged();
    void refreshFinished(bool success);

  private:
    [[nodiscard]] QUrl balanceEndpoint() const;
    [[nodiscard]] QDateTime currentDateTime() const;
    void readReplyData();
    void finishReply();
    void completeFailure(const QString &message);
    void setBusy(bool busy);
    void setError(const QString &error);

    QNetworkAccessManager *m_network = nullptr;
    QUrl m_baseEndpoint{QStringLiteral("https://api.deepseek.com")};
    QMap<QString, QString> m_environment;
    QDateTime m_currentDateTime;
    int m_timeoutMilliseconds = 30'000;
    QVariantMap m_provider;
    QString m_error;
    QByteArray m_responseData;
    QNetworkReply *m_reply = nullptr;
    QTimer m_timeout;
    bool m_busy = false;
    bool m_timedOut = false;
    bool m_responseTooLarge = false;
};

} // namespace Kodometer
