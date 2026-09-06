#pragma once

#include <kodometer/quota_alert_policy.hpp>

namespace Kodometer {

class QuotaNotifier : public QuotaAlertPolicy
{
    Q_OBJECT
    QML_ELEMENT

  public:
    explicit QuotaNotifier(QObject *parent = nullptr);
};

} // namespace Kodometer
