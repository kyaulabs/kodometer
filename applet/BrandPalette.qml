import QtQuick
import org.kde.kirigami as Kirigami

Item {
    property color surfaceColor: Kirigami.Theme.backgroundColor
    property bool systemAccent: false
    readonly property bool darkSurface: surfaceColor.hslLightness < 0.5
    readonly property color logoColor: darkSurface ? "#F7F5FB" : "#252333"
    readonly property color accentColor: systemAccent ? Kirigami.Theme.highlightColor : (
                                                            darkSurface ? "#A28BE0" : "#7052B5")
    readonly property url wordmark: darkSurface ? Qt.resolvedUrl(
                                                      "assets/kodometer-iris-on-dark.svg") :
                                                  Qt.resolvedUrl(
                                                      "assets/kodometer-deep-iris-on-light.svg")
}
