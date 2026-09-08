"""Check the shell-facing tooltip bindings without starting provider credential discovery."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PanelUiTests(unittest.TestCase):
    def test_native_tooltip_is_bound_to_real_provider_data(self):
        main = (ROOT / "applet/main.qml").read_text()
        self.assertIn("toolTipMainText: panelToolTip.mainText", main)
        self.assertIn("toolTipSubText: panelToolTip.subText", main)
        self.assertIn("toolTipTextFormat: Text.PlainText", main)
        summary = main.split("PanelToolTip {", 1)[1].split("\n    }", 1)[0]
        self.assertIn("providers: backend.providers", summary)
        self.assertIn("donutCharts: Plasmoid.configuration.panelDonutCharts", summary)
        compact = main.split("compactRepresentation:", 1)[1].split("fullRepresentation:", 1)[0]
        self.assertNotIn("QQC2.ToolTip", compact)
        self.assertNotIn("hoverEnabled: true", compact)
        self.assertIn("providers: backend.providers", compact)
        self.assertIn("toolTipsEnabled: !root.expanded", compact)


if __name__ == "__main__":
    unittest.main()
