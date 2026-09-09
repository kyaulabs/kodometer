pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    property var series: []
    property var windows: []
    property color accentColor: Kirigami.Theme.highlightColor
    property real endTime: Date.now() / 1000
    property int rangeDays: 1
    property string selectedKind: windows.length ? String(windows[0].kind) : ""
    readonly property var selectedSeries: series.find(item => item.kind === selectedKind)
    readonly property var points: selectedSeries ? selectedSeries.points.filter(point => point[0]
                                                                                         >= endTime
                                                                                         - rangeDays
                                                                                         * 86400
                                                                                         && point[0]
                                                                                         <= endTime) :
                                                   []

    function connects(previous, point) {
        return previous !== null && point[0] - previous[0] <= 900 && point[2] === previous[2]
                && point[1] <= previous[1]
    }

    visible: windows.length > 0
    spacing: Kirigami.Units.smallSpacing

    RowLayout {
        Layout.fillWidth: true
        QQC2.Label {
            Layout.fillWidth: true
            text: qsTr("Quota history")
            font.bold: true
        }
        QQC2.ComboBox {
            objectName: "quotaHistoryRange"
            model: [qsTr("24 hours"), qsTr("7 days"), qsTr("30 days")]
            onActivated: root.rangeDays = [1, 7, 30][currentIndex]
        }
    }

    QQC2.ComboBox {
        objectName: "quotaHistoryWindow"
        Layout.fillWidth: true
        visible: root.windows.length > 1
        model: root.windows.map(windowData => String(windowData.label || windowData.kind))
        currentIndex: Math.max(0, root.windows.findIndex(windowData => windowData.kind
                                                                       === root.selectedKind))
        onActivated: root.selectedKind = String(root.windows[currentIndex].kind)
    }

    onWindowsChanged: {
        if (!windows.some(windowData => windowData.kind === selectedKind))
            selectedKind = windows.length ? String(windows[0].kind) : ""
    }
    onPointsChanged: chart.requestPaint()
    onAccentColorChanged: chart.requestPaint()

    RowLayout {
        Layout.fillWidth: true
        ColumnLayout {
            Layout.preferredHeight: 100
            QQC2.Label {
                text: "100%"
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
            Item {
                Layout.fillHeight: true
            }
            QQC2.Label {
                text: "50%"
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
            Item {
                Layout.fillHeight: true
            }
            QQC2.Label {
                text: "0%"
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
        }
        Canvas {
            id: chart
            objectName: "quotaHistoryPlot"
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Observed remaining quota")
            Accessible.description: root.points.length ? qsTr(
                                                             "%1 observations; latest %2% remaining").arg(
                                                             root.points.length).arg(
                                                             root.points[root.points.length
                                                                         - 1][1]) : qsTr(
                                                             "No observations in this range")
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.lineWidth = 1
                ctx.strokeStyle = Qt.alpha(root.accentColor, 0.15)
                for (let lane = 0; lane <= 4; ++lane) {
                    const y = 1 + lane * (height - 2) / 4
                    ctx.beginPath()
                    ctx.moveTo(0, y)
                    ctx.lineTo(width, y)
                    ctx.stroke()
                }
                const start = root.endTime - root.rangeDays * 86400
                let previous = null
                for (const point of root.points) {
                    const x = (point[0] - start) / (root.rangeDays * 86400) * width
                    const y = 1 + (100 - point[1]) / 100 * (height - 2)
                    ctx.strokeStyle = root.accentColor
                    ctx.fillStyle = root.accentColor
                    ctx.lineWidth = 1.5
                    if (root.connects(previous, point)) {
                        const px = (previous[0] - start) / (root.rangeDays * 86400) * width
                        const py = 1 + (100 - previous[1]) / 100 * (height - 2)
                        ctx.beginPath()
                        ctx.moveTo(px, py)
                        ctx.lineTo(x, y)
                        ctx.stroke()
                        ctx.fillStyle = Qt.alpha(root.accentColor, 0.06)
                        ctx.lineTo(x, height)
                        ctx.lineTo(px, height)
                        ctx.closePath()
                        ctx.fill()
                    } else {
                        ctx.beginPath()
                        ctx.arc(x, y, 1.5, 0, 2 * Math.PI)
                        ctx.fill()
                    }
                    previous = point
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        QQC2.Label {
            Layout.fillWidth: true
            text: new Date((root.endTime - root.rangeDays * 86400) * 1000).toLocaleString(Qt.locale(
                                                                                              ), "MMM d hh:mm")
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }
        QQC2.Label {
            text: qsTr("Now")
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }
    }
    QQC2.Label {
        Layout.fillWidth: true
        text: root.points.length ? qsTr(
                                       "Observed remaining quota · gaps are not zero. Resets start a new line.") :
                                   qsTr("No observations in this range. History builds from successful refreshes, not past activity.")
        wrapMode: Text.WordWrap
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
    }
}
