#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

namespace Kodometer {

class QuotaHistory : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool persistent READ persistent WRITE setPersistent NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

  public:
    explicit QuotaHistory(QObject *parent = nullptr);
    [[nodiscard]] bool persistent() const;
    void setPersistent(bool persistent);
    void setDirectory(const QString &directory);
    [[nodiscard]] QString error() const;
    [[nodiscard]] int revision() const;
    Q_INVOKABLE QVariantList view(const QString &provider) const;
    Q_INVOKABLE bool clear();
    void prepareContext(const QString &provider, const QByteArray &scope,
                        const QList<QByteArray> &identities);
    void forgetProvider(const QString &provider);
    void observe(const QString &provider, const QByteArray &scope,
                 const QList<QByteArray> &identities, const QVariantList &windows,
                 const QDateTime &now = QDateTime::currentDateTimeUtc());

  signals:
    void changed();

  private:
    bool load(int directory, const QDateTime &now);
    bool save(int directory);
    void publish();
    QString m_directory;
    QString m_error;
    bool m_persistent = false;
    int m_revision = 0;
    QJsonObject m_streams;
    QMap<QString, QString> m_selected;
};

} // namespace Kodometer
