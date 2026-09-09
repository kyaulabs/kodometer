"""Check the shell-facing tooltip bindings without starting provider credential discovery."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PanelUiTests(unittest.TestCase):
    def test_coverage_installs_the_real_plasma_tooltip_module(self):
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        coverage = workflow.split("  coverage:\n", 1)[1].split("  security:\n", 1)[0]
        self.assertIn("libplasma", coverage.split("- name: Check out repository", 1)[0])
        self.assertIn("dbus", coverage.split("- name: Check out repository", 1)[0])
        self.assertIn("run: scripts/coverage.sh", coverage)

    def test_visual_tests_use_isolated_desktop_state(self):
        cmake = (ROOT / "tests/CMakeLists.txt").read_text()
        self.assertIn('COMMAND bash "${PROJECT_SOURCE_DIR}/scripts/test-qml.sh"', cmake)
        script = (ROOT / "scripts/test-qml.sh").read_text()
        for key in ["HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_RUNTIME_DIR"]:
            self.assertIn(f'export {key}="$work/', script)
        self.assertIn('dbus-run-session -- "$@"', script)

    def test_native_tooltip_is_bound_to_real_provider_data(self):
        main = (ROOT / "applet/main.qml").read_text()
        self.assertIn("toolTipMainText: panelToolTip.mainText", main)
        self.assertIn("toolTipSubText: panelToolTip.subText", main)
        self.assertIn("toolTipTextFormat: Text.PlainText", main)
        summary = main.split("PanelToolTip {", 1)[1].split("\n    }", 1)[0]
        self.assertIn("providers: presentation.displayedProviders", summary)
        self.assertIn("donutCharts: false", summary)
        compact = main.split("compactRepresentation:", 1)[1].split("fullRepresentation:", 1)[0]
        self.assertNotIn("QQC2.ToolTip", compact)
        self.assertNotIn("hoverEnabled: true", compact)
        self.assertIn("providers: presentation.displayedProviders", compact)
        self.assertIn("toolTipsEnabled: !root.expanded", compact)


if __name__ == "__main__":
    unittest.main()
