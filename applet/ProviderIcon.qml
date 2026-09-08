pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

Item {
    id: root

    property string providerId
    readonly property bool knownProvider: ["codex", "claude", "deepseek", "gemini", "kimi",
        "openrouter", "xai", "zai"].includes(providerId)
    // The supplied white OpenAI and lime OpenRouter marks need a dark backing
    // even on light desktops. Other assets retain their original colors/tiles.
    readonly property bool needsBacking: providerId === "codex" || providerId === "openrouter"
    readonly property url iconSource: knownProvider ? Qt.resolvedUrl("assets/providers/"
                                                                     + providerId + ".svg") : ""
    readonly property alias status: artwork.status

    implicitWidth: Kirigami.Units.iconSizes.medium
    implicitHeight: implicitWidth

    Rectangle {
        anchors.fill: parent
        visible: root.needsBacking
        color: "#252333"
        radius: width * 0.18
    }

    Image {
        id: artwork
        anchors.fill: parent
        anchors.margins: root.needsBacking ? parent.width * 0.12 : 0
        source: root.iconSource
        sourceSize.width: Math.ceil(width * Screen.devicePixelRatio)
        sourceSize.height: Math.ceil(height * Screen.devicePixelRatio)
        fillMode: Image.PreserveAspectFit
    }

    Kirigami.Icon {
        anchors.fill: parent
        visible: !root.knownProvider
        source: "image-missing"
    }
}
