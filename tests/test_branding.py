"""Verify supplied artwork provenance and lossless square provider canvases."""
import hashlib
import json
from pathlib import Path
import re
import tomllib
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "branding/kodometer-brand-pack"


class BrandingTests(unittest.TestCase):
    def test_original_pack_checksums(self):
        manifest = json.loads((PACK / "checksums.json").read_text())
        files = {str(p.relative_to(PACK)) for p in PACK.rglob("*") if p.is_file()}
        self.assertEqual(files, set(manifest) | {"checksums.json"})
        for name, expected in manifest.items():
            with self.subTest(name=name):
                self.assertEqual(hashlib.sha256((PACK / name).read_bytes()).hexdigest(), expected)

    def test_checksum_scanner_exception_is_rule_and_value_scoped(self):
        config = tomllib.loads((ROOT / ".gitleaks.toml").read_text())
        self.assertTrue(config["extend"]["useDefault"])
        # Global path allowlists can skip entire files in directory scans.
        self.assertNotIn("allowlists", config)
        self.assertNotIn("allowlist", config)
        self.assertEqual(len(config["rules"]), 1)
        rule = config["rules"][0]
        self.assertEqual(rule["id"], "generic-api-key")
        self.assertEqual(len(rule["allowlists"]), 1)
        exception = rule["allowlists"][0]
        self.assertEqual(exception["condition"], "AND")
        self.assertEqual(exception["regexTarget"], "line")
        path = exception["paths"][0]
        self.assertIsNotNone(re.fullmatch(path, "branding/kodometer-brand-pack/checksums.json"))
        self.assertIsNone(re.fullmatch(path, "applet/credentials.json"))
        pattern = exception["regexes"][0]
        manifest = json.loads((PACK / "checksums.json").read_text())
        row = f'"website/tokens.json": "{manifest["website/tokens.json"]}"'
        self.assertIsNotNone(re.fullmatch(pattern, row))
        self.assertIsNone(re.search(pattern, 'api_key = "unrelated-synthetic-test-value"'))
        self.assertIsNone(re.search(pattern, row + ', "api_key": "unrelated-synthetic-test-value"'))
        self.assertIsNone(re.fullmatch(pattern, row.replace(manifest["website/tokens.json"], "0" * 64)))

    def test_runtime_brand_masters_are_unchanged(self):
        for category, name in [("plasma", "kodometer-symbolic.svg"),
                               ("logos", "kodometer-iris-on-dark.svg"),
                               ("logos", "kodometer-deep-iris-on-light.svg")]:
            self.assertEqual((ROOT / "applet/assets" / name).read_bytes(),
                             (PACK / category / name).read_bytes())

    def test_provider_canvases_are_centered_without_changing_artwork(self):
        originals = ROOT / "branding/provider-icons"
        runtime = ROOT / "applet/assets/providers"
        self.assertEqual({p.stem for p in runtime.glob("*.svg")},
                         {"codex", "claude", "deepseek", "gemini", "kimi", "openrouter", "xai", "zai"})
        for source in originals.glob("*.svg"):
            with self.subTest(name=source.name):
                original = ET.parse(source).getroot()
                name = "codex.svg" if source.name == "openai.svg" else source.name
                normalized = ET.parse(runtime / name).getroot()
                x, y, w, h = map(float, original.attrib["viewBox"].split())
                nx, ny, nw, nh = map(float, normalized.attrib["viewBox"].split())
                self.assertEqual(nw, max(w, h))
                self.assertEqual(nw, nh)
                self.assertAlmostEqual(nx + nw / 2, x + w / 2)
                self.assertAlmostEqual(ny + nh / 2, y + h / 2)
                self.assertEqual(float(normalized.attrib["width"]), nw)
                self.assertEqual(float(normalized.attrib["height"]), nh)
                self.assertEqual([ET.tostring(e) for e in original],
                                 [ET.tostring(e) for e in normalized])


if __name__ == "__main__":
    unittest.main()
