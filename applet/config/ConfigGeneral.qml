pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
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
                                                                    === "string" && !
                                                                    /^codex\/.*spark/i.test(
                                                                        entry.key)).slice(0, 128) :
                                            []
        } catch (error) {
            return []
        }
    }
    property var cfg_disabledProviders: []
    property string cfg_providerColors: "{}"
    readonly property var providerColors: {
        try {
            const colors = JSON.parse(cfg_providerColors)
            const result = {}
            if (colors && typeof colors === "object" && !Array.isArray(colors)) {
                for (const id of Object.keys(defaultColors)) {
                    if (typeof colors[id] === "string" && /^#[0-9a-fA-F]{6}$/.test(colors[id]))
                        result[id] = colors[id]
                }
            }
            return result
        } catch (error) {
            return ({})
        }
    }
    readonly property var defaultColors: ({
                                              codex: "#A28BE0",
                                              claude: "#D97757",
                                              gemini: "#4285F4",
                                              xai: "#8E8E93",
                                              kimi: "#49A3B0",
                                              deepseek: "#4D6BFE",
                                              zai: "#E85A6A",
                                              openrouter: "#C8FF00"
                                          })

    function setProviderColor(id, value) {
        const colors = Object.assign({}, providerColors)
        if (/^#[0-9a-fA-F]{6}$/.test(value))
            colors[id] = value
        else
            delete colors[id]
        cfg_providerColors = JSON.stringify(colors)
    }
    property bool cfg_panelDonutCharts: false
    property bool cfg_panelSystemAccent: false
    property alias cfg_quotaNotifications: notifications.checked
    property alias cfg_quotaNotificationThreshold: notificationThreshold.value

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
                      "Window switches apply to all quota displays, not notifications. Available windows appear after a successful refresh. The single Spark switch controls all Codex Spark windows; idle windows also require their display switch above.")
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

            delegate: ColumnLayout {
                id: providerSettings
                required property var modelData
                required property int index
                Kirigami.FormData.label: index === 0 ? qsTr("Providers:") : ""

                QQC2.CheckBox {
                    objectName: "provider-" + providerSettings.modelData.id
                    text: providerSettings.modelData.name
                    checked: !root.cfg_disabledProviders.includes(providerSettings.modelData.id)
                    onClicked: {
                        const disabled = root.cfg_disabledProviders.filter(id => id
                                                                                 !== providerSettings.modelData.id)
                        if (!checked)
                            disabled.push(providerSettings.modelData.id)
                        root.cfg_disabledProviders = disabled
                    }
                }
                ChartColorControl {
                    id: providerColor
                    objectName: "provider-color-" + providerSettings.modelData.id
                    fallbackColor: root.defaultColors[providerSettings.modelData.id]
                    resetText: qsTr("Use default color")
                    onColorValueChanged: {
                        const saved = root.providerColors[providerSettings.modelData.id] || ""
                        if (colorValue !== saved)
                            root.setProviderColor(providerSettings.modelData.id, colorValue)
                    }
                    Binding {
                        target: providerColor
                        property: "colorValue"
                        value: root.providerColors[providerSettings.modelData.id] || ""
                    }
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
