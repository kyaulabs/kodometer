pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root
    title: qsTr("General")
    verticalScrollBarPolicy: flickable.contentHeight > flickable.height ? QQC2.ScrollBar.AlwaysOn :
                                                                          QQC2.ScrollBar.AlwaysOff

    readonly property QQC2.ScrollView scrollView: contentItem as QQC2.ScrollView

    Binding {
        target: root.scrollView.QQC2.ScrollBar.vertical
        property: "visible"
        value: root.verticalScrollBarPolicy === QQC2.ScrollBar.AlwaysOn
    }

    property alias cfg_autoRefresh: automatic.checked
    property alias cfg_refreshIntervalMinutes: refreshInterval.value
    property alias cfg_showIdleWindows: idleWindows.checked
    property alias cfg_showCodexSpark: spark.checked
    property alias cfg_quotaBarsRemaining: remainingBars.checked
    property var cfg_hiddenQuotaWindows: []
    property string cfg_quotaWindowCatalog: "[]"
    readonly property var quotaWindows: {
        try {
            const entries = JSON.parse(cfg_quotaWindowCatalog)
            return Array.isArray(entries) ? entries.filter(entry => entry && typeof entry.key
                                                                    === "string"
                                                                    && typeof entry.provider
                                                                    === "string"
                                                                    && typeof entry.label
                                                                    === "string").slice(0, 128) : []
        } catch (error) {
            return []
        }
    }
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

        QQC2.CheckBox {
            id: spark
            objectName: "showCodexSpark"
            text: qsTr("Show Codex Spark quota")
        }

        QQC2.CheckBox {
            id: remainingBars
            objectName: "quotaBarsRemaining"
            text: qsTr("Fill bars with remaining quota (instead of used)")
            checked: true
        }

        Repeater {
            model: root.quotaWindows
            delegate: QQC2.CheckBox {
                id: windowSwitch
                required property var modelData
                contentItem: QQC2.Label {
                    text: windowSwitch.text
                    textFormat: Text.PlainText
                    leftPadding: windowSwitch.mirrored ? 0 : windowSwitch.indicator.width
                                                         + windowSwitch.spacing
                    rightPadding: windowSwitch.mirrored ? windowSwitch.indicator.width
                                                          + windowSwitch.spacing : 0
                    verticalAlignment: Text.AlignVCenter
                }
                objectName: "quota-window-" + modelData.key
                text: modelData.provider + " — " + modelData.label
                checked: !root.cfg_hiddenQuotaWindows.includes(modelData.key)
                onClicked: {
                    const hidden = root.cfg_hiddenQuotaWindows.filter(key => key !== modelData.key)
                    if (!checked)
                        hidden.push(modelData.key)
                    root.cfg_hiddenQuotaWindows = hidden
                }
            }
        }

        QQC2.Label {
            text: qsTr(
                      "Window switches apply to all quota displays, not notifications. Available windows appear after a successful refresh. Spark and idle windows also require their display switches above.")
            wrapMode: Text.WordWrap
            Kirigami.FormData.isSection: true
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
            Kirigami.FormData.label: qsTr("Bar meter accent:")
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
