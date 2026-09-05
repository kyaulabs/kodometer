pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: qsTr("Credentials")
        icon: "wallet-open"
        source: "ConfigCredentials.qml"
    }
}
