#pragma once

#include <kodometer/credential_store.hpp>

#include <QQmlEngine>

namespace Kodometer {

class KWalletCredentialStore : public CredentialStore
{
    Q_OBJECT
    QML_ELEMENT

  public:
    explicit KWalletCredentialStore(QObject *parent = nullptr);
};

} // namespace Kodometer
