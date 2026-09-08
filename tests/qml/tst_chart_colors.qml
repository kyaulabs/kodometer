import QtQuick
import QtQuick.Dialogs
import QtTest
import "../../applet/config" as Config

TestCase {
    name: "ChartColors"
    visible: true
    when: windowShown
    width: 500
    height: 500

    Component {
        id: controlComponent
        Config.ChartColorControl {
            width: 400
            fallbackColor: "#7052b5"
        }
    }

    function test_pickerAcceptCancelAndReset() {
        const control = createTemporaryObject(controlComponent, this)
        const picker = findChild(control, "colorPicker")
        const choose = findChild(control, "chooseColor")
        const reset = findChild(control, "resetColor")
        picker.options = ColorDialog.DontUseNativeDialog
        compare(control.colorValue, "")
        compare(reset.enabled, false)
        choose.click()
        tryCompare(picker, "visible", true)
        compare(picker.selectedColor, "#7052b5")
        picker.selectedColor = "#112233"
        picker.reject()
        tryCompare(picker, "visible", false)
        compare(control.colorValue, "")
        choose.click()
        tryCompare(picker, "visible", true)
        compare(picker.selectedColor, "#7052b5")
        picker.selectedColor = "#aabbcc"
        picker.accept()
        tryCompare(picker, "visible", false)
        compare(control.colorValue, "#aabbcc")
        compare(control.customColor, true)
        choose.click()
        tryCompare(picker, "visible", true)
        compare(picker.selectedColor, "#aabbcc")
        picker.reject()
        reset.click()
        compare(control.colorValue, "")
        control.colorValue = "invalid"
        compare(control.customColor, false)
        choose.click()
        tryCompare(picker, "visible", true)
        compare(picker.selectedColor, "#7052b5")
        picker.reject()
    }
}
