import QtQuick
import QtTest
import org.kde.kirigami as Kirigami
import "../../applet" as Applet

TestCase {
    name: "Branding"
    when: windowShown
    visible: true
    width: 640
    height: 480

    Component {
        id: paletteComponent
        Applet.BrandPalette {}
    }

    Component {
        id: iconComponent
        Applet.ProviderIcon {
            width: 48
            height: 48
        }
    }

    Component {
        id: imageComponent
        Image {
            width: 200
            height: 64
            fillMode: Image.PreserveAspectFit
        }
    }

    function test_paletteFollowsSurfaceAndSystemChoice() {
        const palette = createTemporaryObject(paletteComponent, this)
        palette.surfaceColor = "#ffffff"
        compare(palette.accentColor, "#7052b5")
        compare(palette.logoColor, "#252333")
        verify(String(palette.wordmark).endsWith("kodometer-deep-iris-on-light.svg"))
        let image = createTemporaryObject(imageComponent, this, {
                                              source: palette.wordmark
                                          })
        tryCompare(image, "status", Image.Ready)
        palette.surfaceColor = "#252333"
        compare(palette.accentColor, "#a28be0")
        compare(palette.logoColor, "#f7f5fb")
        verify(String(palette.wordmark).endsWith("kodometer-iris-on-dark.svg"))
        image.source = palette.wordmark
        tryCompare(image, "status", Image.Ready)
        palette.systemAccent = true
        compare(palette.accentColor, Kirigami.Theme.highlightColor)
        palette.systemAccent = false
        compare(palette.accentColor, "#a28be0")
    }

    function test_providerAssets_data() {
        return ["codex", "claude", "deepseek", "gemini", "kimi", "openrouter", "xai", "zai"].map(id => (
        {

            tag: id,
            id: id
        }))
    }

        function test_providerAssets(data) {
        const icon = createTemporaryObject(iconComponent, this, {
        providerId: data.id
    })
        verify(icon.knownProvider)
        tryCompare(icon, "status", Image.Ready)
        compare(icon.needsBacking, data.id === "codex" || data.id === "openrouter")
        verify(waitForRendering(icon))
        const pixels = grabImage(icon)
        verify(pixels.width > 0)
        // Every supplied asset has visible artwork in its center region.
        let opaque = false
        for (let y = 12; y < 36; ++y)
        for (let x = 12; x < 36; ++x)
        opaque = opaque || pixels.alpha(x, y) > 0
        verify(opaque)
    }

        function test_unknownProviderCannotChooseAFile() {
        const icon = createTemporaryObject(iconComponent, this, {
        providerId: "../../private"
    })
        compare(icon.knownProvider, false)
        compare(String(icon.iconSource), "")
        icon.providerId = ""
        compare(icon.knownProvider, false)
    }
    }
