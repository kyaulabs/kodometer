pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root
    title: qsTr("General")

    property alias cfg_autoRefresh: automatic.checked
    property alias cfg_refreshIntervalMinutes: refreshInterval.value
    property alias cfg_showIdleWindows: idleWindows.checked
    property var cfg_disabledProviders: []
    property bool cfg_panelDonutCharts: false
    property bool cfg_panelSystemAccent: false
    property alias cfg_panelSessionColor: sessionColor.colorValue
    property alias cfg_panelWeeklyColor: weeklyColor.colorValue
    property alias cfg_quotaNotifications: notifications.checked
    property alias cfg_quotaNotificationThreshold: notificationThreshold.value

    // Configuration resources are flattened by ECM; keep the preview on the same
    // documented Iris colors as BrandPalette without a source-only relative import.
    readonly property color meterAccent: cfg_panelSystemAccent ? Kirigami.Theme.highlightColor : (
                                                                     Kirigami.Theme.backgroundColor.hslLightness
                                                                     < 0.5 ? "#A28BE0" : "#7052B5")

    Kirigami.FormLayout {
        QQC2.CheckBox {
            id: automatic
            Kirigami.FormData.label: qsTr("Refresh:")
            text: qsTr("Refresh automatically")
            checked: true
        }

        QQC2.SpinBox {
            id: refreshInterval
            objectName: "refreshInterval"
            Kirigami.FormData.label: qsTr("Interval (minutes):")
            from: 1
            to: 1440
            value: 5
            editable: true
            enabled: automatic.checked
        }

        QQC2.CheckBox {
            id: idleWindows
            objectName: "showIdleWindows"
            Kirigami.FormData.label: qsTr("Display:")
            text: qsTr("Show idle quota windows")
        }

        QQC2.ComboBox {
            id: chartStyle
            objectName: "panelDonutCharts"
            Kirigami.FormData.label: qsTr("Panel meters:")
            model: [qsTr("Bars"), qsTr("Donut charts")]
            currentIndex: root.cfg_panelDonutCharts ? 1 : 0
            onActivated: root.cfg_panelDonutCharts = currentIndex === 1
        }

        QQC2.ComboBox {
            id: meterAccent
            objectName: "panelSystemAccent"
            Kirigami.FormData.label: qsTr("Meter accent:")
            model: [qsTr("Kodometer Iris"), qsTr("Desktop accent")]
            currentIndex: root.cfg_panelSystemAccent ? 1 : 0
            onActivated: root.cfg_panelSystemAccent = currentIndex === 1
        }

        ChartColorControl {
            id: sessionColor
            objectName: "panelSessionColor"
            Kirigami.FormData.label: qsTr("Session donut color:")
            enabled: root.cfg_panelDonutCharts
            fallbackColor: root.meterAccent
        }

        ChartColorControl {
            id: weeklyColor
            objectName: "panelWeeklyColor"
            Kirigami.FormData.label: qsTr("Weekly donut color:")
            enabled: root.cfg_panelDonutCharts
            fallbackColor: root.meterAccent
        }

        QQC2.CheckBox {
            id: notifications
            Kirigami.FormData.label: qsTr("Notifications:")
            text: qsTr("Notify when quota is low")
        }

        QQC2.SpinBox {
            id: notificationThreshold
            objectName: "quotaNotificationThreshold"
            Kirigami.FormData.label: qsTr("Remaining quota (%):")
            from: 1
            to: 50
            value: 10
            editable: true
            enabled: notifications.checked
        }

        Repeater {
            model: [
                {
                    "id": "codex",
                    "name": "Codex"
                },
                {
                    "id": "claude",
                    "name": "Claude"
                },
                {
                    "id": "gemini",
                    "name": "Gemini"
                },
                {
                    "id": "xai",
                    "name": "xAI"
                },
                {
                    "id": "kimi",
                    "name": "Kimi Code"
                },
                {
                    "id": "deepseek",
                    "name": "DeepSeek"
                },
                {
                    "id": "zai",
                    "name": "z.ai"
                },
                {
                    "id": "openrouter",
                    "name": "OpenRouter"
                }
            ]

            delegate: QQC2.CheckBox {
                id: providerSwitch
                required property var modelData
                required property int index
                objectName: "provider-" + modelData.id
                Kirigami.FormData.label: index === 0 ? qsTr("Enabled providers:") : ""
                text: modelData.name
                checked: !root.cfg_disabledProviders.includes(modelData.id)
                onClicked: {
                    const disabled = root.cfg_disabledProviders.filter(id => id !== modelData.id)
                    if (!checked) {
                        disabled.push(modelData.id)
                    }
                    root.cfg_disabledProviders = disabled
                }
            }
        }

        QQC2.Label {
            text: qsTr(
                      "Disabled providers are hidden and skipped on future refreshes. Requests already in progress may finish. Credentials are kept in KWallet.")
            wrapMode: Text.WordWrap
            Kirigami.FormData.isSection: true
        }
    }
}
