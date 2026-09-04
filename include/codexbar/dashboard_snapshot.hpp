#pragma once

#include <QByteArrayView>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

#include <optional>

namespace CodexBar {

class DashboardSnapshot final
{
  public:
    static constexpr qsizetype MaximumPayloadBytes = 4 * 1024 * 1024;
    static constexpr int SupportedSchemaVersion = 1;

    [[nodiscard]] static std::optional<DashboardSnapshot> fromJson(QByteArrayView payload,
                                                                   QString *error = nullptr);

    [[nodiscard]] int schemaVersion() const noexcept;
    [[nodiscard]] QDateTime generatedAt() const;
    [[nodiscard]] int staleAfterSeconds() const noexcept;
    [[nodiscard]] QJsonArray providers() const;
    [[nodiscard]] std::optional<QJsonObject> provider(const QString &id) const;
    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] bool isStale(const QDateTime &now = QDateTime::currentDateTimeUtc()) const;

  private:
    DashboardSnapshot(QJsonObject document, QDateTime generatedAt, int staleAfterSeconds);

    QJsonObject m_document;
    QDateTime m_generatedAt;
    int m_staleAfterSeconds;
};

} // namespace CodexBar
