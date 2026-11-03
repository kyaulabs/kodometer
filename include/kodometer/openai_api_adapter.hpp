#pragma once

#include <QDateTime>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <kodometer/provider_adapter.hpp>

class QNetworkAccessManager;
namespace Kodometer {
class OpenAIApiAdapter : public ProviderAdapter
{
    Q_OBJECT
  public:
    explicit OpenAIApiAdapter(QNetworkAccessManager *network = nullptr, QObject *parent = nullptr);
    [[nodiscard]] QString providerId() const override;
    [[nodiscard]] QByteArray defaultCredentialContext() const override;
    void refresh() override;
    void setEndpoint(const QUrl &endpoint);
    void setTimeoutMilliseconds(int milliseconds);

  private:
    QMap<QString, QString> environment() const;
    void readData();
    void finish();
    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_timeout;
    QUrl m_endpoint{QStringLiteral("https://api.openai.com/v1/organization/costs")};
    QDateTime m_now;
    qint64 m_start = 0;
    QByteArray m_data;
    int m_timeoutMilliseconds = 15000;
    bool m_timedOut = false;
    bool m_oversized = false;
};
} // namespace Kodometer
