pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

Item {
    id: root

    property real remaining: NaN
    property color accentColor: Kirigami.Theme.highlightColor
    property string quotaLabel
    readonly property bool available: Number.isFinite(remaining)
    readonly property real value: available ? Math.max(0, Math.min(100, remaining)) : 0
    readonly property string valueText: available ? qsTr("%1%").arg(value.toFixed(1)) : "—"
    readonly property string remainingText: available ? qsTr("%1 remaining").arg(valueText) : qsTr(
                                                            "Not reported")
    readonly property real strokeWidth: Math.max(2, Math.min(width, height) * 0.09)

    implicitWidth: 48
    implicitHeight: 48
    Accessible.role: Accessible.ProgressBar
    Accessible.name: quotaLabel
    Accessible.description: remainingText

    onValueChanged: ring.requestPaint()
    onAccentColorChanged: ring.requestPaint()
    onStrokeWidthChanged: ring.requestPaint()

    Canvas {
        id: ring
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const radius = Math.max(0, (Math.min(width, height) - root.strokeWidth) / 2)
            if (radius === 0)
                return
            ctx.lineWidth = root.strokeWidth
            ctx.lineCap = "round"
            ctx.strokeStyle = Qt.alpha(root.accentColor, 0.22)
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, radius, 0, 2 * Math.PI)
            ctx.stroke()
            if (root.value > 0) {
                ctx.strokeStyle = root.accentColor
                ctx.beginPath()
                ctx.arc(width / 2, height / 2, radius, -Math.PI / 2, -Math.PI / 2 + 2 * Math.PI
                        * root.value / 100)
                ctx.stroke()
            }
        }
    }

    Text {
        anchors.centerIn: parent
        width: Math.max(0, parent.width - root.strokeWidth * 3)
        height: parent.height / 2
        visible: parent.width >= 32
        text: root.valueText
        textFormat: Text.PlainText
        color: Kirigami.Theme.textColor
        font: Kirigami.Theme.smallFont
        fontSizeMode: Text.Fit
        minimumPointSize: 6
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
