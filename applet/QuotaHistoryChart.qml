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

    readonly property real startTime: endTime - rangeDays * 86400
    readonly property var observations: selectedSeries ? selectedSeries.points : []
    readonly property var plotPoints: {
        const result = []
        let value = valueAt(startTime)
        if (value !== null)
            result.push([startTime, value])
        for (const point of points) {
            if (value !== null)
                result.push([point[0], value])
            result.push([point[0], point[1]])
            value = point[1]
        }
        if (value !== null)
            result.push([endTime, value])
        return result
    }
    readonly property real hoverTime: startTime + pointer.mouseX / chart.width * rangeDays * 86400
    readonly property var hoverValue: pointer.containsMouse ? valueAt(hoverTime) : null

    // Carry the last observation only in the view, never in the persisted history.
    function valueAt(time) {
        let value = null
        for (const point of observations) {
            if (point[0] > time)
                break
            value = point[1]
        }
        return value
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
    onPlotPointsChanged: chart.requestPaint()
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
            Accessible.description: root.plotPoints.length ? qsTr(
                                                                 "%1 observations; latest %2% remaining").arg(
                                                                 root.points.length).arg(
                                                                 root.plotPoints[root.plotPoints.length
                                                                                 - 1][1]) : qsTr(
                                                                 "No observations in this range")
            MouseArea {
                id: pointer
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
            Rectangle {
                objectName: "quotaHistoryCursor"
                visible: root.hoverValue !== null
                x: Math.min(chart.width - width, Math.max(0, pointer.mouseX))
                width: 1
                height: chart.height
                color: Qt.alpha(root.accentColor, 0.5)
            }
            Rectangle {
                visible: root.hoverValue !== null
                x: pointer.mouseX - width / 2
                y: 1 + (100 - Number(root.hoverValue)) / 100 * (chart.height - 2) - height / 2
                width: 6
                height: width
                radius: width / 2
                color: root.accentColor
            }
            QQC2.ToolTip {
                objectName: "quotaHistoryTooltip"
                visible: root.hoverValue !== null
                x: Math.max(0, Math.min(chart.width - implicitWidth, pointer.mouseX - implicitWidth
                                        / 2))
                y: -implicitHeight - 4
                text: qsTr("%1% remaining · %2\nLast observed value").arg(root.hoverValue === null
                                                                          ? "" : Number(
                                                                                root.hoverValue).toLocaleString(
                                                                                Qt.locale(), 'f',
                                                                                1).replace(
                                                                                /([.,]0)$/, "")).arg(
                          new Date(root.hoverTime * 1000).toLocaleString(Qt.locale(),
                                                                         "MMM d hh:mm"))
            }
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
                if (!root.plotPoints.length)
                    return
                const xAt = time => (time - root.startTime) / (root.rangeDays * 86400) * width
                ctx.beginPath()
                for (let i = 0; i < root.plotPoints.length; ++i) {
                    const point = root.plotPoints[i]
                    const x = xAt(point[0])
                    const y = 1 + (100 - point[1]) / 100 * (height - 2)
                    if (i === 0)
                        ctx.moveTo(x, y)
                    else
                        ctx.lineTo(x, y)
                }
                ctx.strokeStyle = root.accentColor
                ctx.lineWidth = 1.5
                ctx.stroke()
                ctx.lineTo(width, height)
                ctx.lineTo(xAt(root.plotPoints[0][0]), height)
                ctx.closePath()
                ctx.fillStyle = Qt.alpha(root.accentColor, 0.06)
                ctx.fill()
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
        text: root.plotPoints.length ? qsTr(
                                           "Observed remaining quota · holds the last value between refreshes. Resets are vertical steps.") :
                                       qsTr("No observations in this range. History builds from successful refreshes, not past activity.")
        wrapMode: Text.WordWrap
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
    }
}
