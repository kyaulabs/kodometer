#pragma once

#include <QJsonObject>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantMap>

#include <optional>

namespace Kodometer {

class OAuthProfiles : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(
        QString configuration READ configuration WRITE setConfiguration NOTIFY configurationChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY configurationChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantMap providers READ providers NOTIFY configurationChanged)

  public:
    explicit OAuthProfiles(QObject *parent = nullptr);
    [[nodiscard]] QString configuration() const;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QVariantMap providers() const;
    void setConfiguration(const QString &configuration);

    [[nodiscard]] static bool supportsProvider(const QString &provider);
    [[nodiscard]] QVariantList entries(const QString &provider) const;
    [[nodiscard]] QString selectedId(const QString &provider) const;
    [[nodiscard]] QString selectedName(const QString &provider) const;
    [[nodiscard]] QString selectedDirectory(const QString &provider) const;
    [[nodiscard]] QString contextKey(const QString &provider) const;
    Q_INVOKABLE bool addProfile(const QString &provider, const QString &name,
                                const QString &directory);
    Q_INVOKABLE bool removeProfile(const QString &provider, const QString &id);
    Q_INVOKABLE bool selectProfile(const QString &provider, const QString &id);
    Q_INVOKABLE QString localDirectory(const QUrl &url) const;

  signals:
    void configurationChanged();
    void errorChanged();
    void contextChanged(const QString &provider);

  private:
    [[nodiscard]] static std::optional<QJsonObject> parse(const QString &configuration);
    [[nodiscard]] QVariantMap selected(const QString &provider) const;
    bool commit(const QJsonObject &document);
    void setError(const QString &error);

    QString m_configuration = QStringLiteral("{}");
    QJsonObject m_document;
    QString m_error;
    bool m_valid = true;
};

} // namespace Kodometer
