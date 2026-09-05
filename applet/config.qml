pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: qsTr("General")
        icon: "configure"
        source: "ConfigGeneral.qml"
    }

    ConfigCategory {
        name: qsTr("Credentials")
        icon: "wallet-open"
        source: "ConfigCredentials.qml"
    }
}
