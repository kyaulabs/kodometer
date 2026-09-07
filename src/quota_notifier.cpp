#include <kodometer/quota_notifier.hpp>

#include <KNotification>

namespace Kodometer {

QuotaNotifier::QuotaNotifier(QObject *parent) : QuotaAlertPolicy(parent)
{
    connect(this, &QuotaAlertPolicy::alertReady, this,
            [this](const QString &title, const QString &body) {
                auto *notification = new KNotification(QStringLiteral("lowQuota"),
                                                       KNotification::CloseOnTimeout, this);
                notification->setComponentName(QStringLiteral("kodometer"));
                notification->setTitle(title);
                notification->setText(body);
                notification->setIconName(QStringLiteral("view-statistics"));
                notification->setUrgency(KNotification::NormalUrgency);
                notification->sendEvent();
            });
}

} // namespace Kodometer
