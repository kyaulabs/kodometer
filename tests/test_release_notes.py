"""Keep current release overviews and generated changelogs on one reader-facing schema."""
import importlib.util
from pathlib import Path
import re
import tomllib
import unittest

ROOT = Path(__file__).resolve().parents[1]
SECTIONS = [
    "✨ Highlights",
    "🚀 Features",
    "🐛 Fixes",
    "📦 Packages",
    "🛠️ Installation and upgrade",
    "⚠️ Compatibility and known limitations",
    "🔒 Security and privacy",
    "✅ Validation",
]


class ReleaseNotesTest(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location("package_metadata", ROOT / "scripts/package_metadata.py")
        self.metadata = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.metadata)
        self.version = self.metadata.project_version()
        self.notes = (ROOT / "docs/releases" / f"{self.version}.md").read_text()

    def test_current_overview_has_fixed_nonempty_sections(self):
        self.assertTrue(self.notes.startswith(f"# 🚀 Kodometer {self.version}\n"))
        self.assertEqual(re.findall(r"^## (.+)$", self.notes, re.MULTILINE), SECTIONS)
        for section in SECTIONS:
            body = self.notes.split(f"## {section}\n", 1)[1].split("\n## ", 1)[0]
            self.assertTrue(body.strip(), section)
        self.assertNotIn("X.Y.Z", self.notes)

    def test_template_has_the_same_sections(self):
        template = (ROOT / "docs/releases/TEMPLATE.md").read_text()
        self.assertTrue(template.startswith("# 🚀 Kodometer X.Y.Z\n"))
        self.assertEqual(re.findall(r"^## (.+)$", template, re.MULTILINE), SECTIONS)

    def test_package_table_covers_inventory_and_native_targets(self):
        section = self.notes.split("## 📦 Packages\n", 1)[1].split("\n## ", 1)[0]
        payloads = self.metadata.payloads(self.version)
        names = re.findall(r"`(kodometer[-_][^`]+)`", section)
        self.assertCountEqual(names, payloads + [self.metadata.manifest_name(self.version)])
        rows = [line for line in section.splitlines() if line.startswith("|")]
        for payload in payloads:
            self.assertEqual(sum(f"`{payload}`" in row for row in rows), 1, payload)
        for target in self.metadata.TARGETS:
            if target["kind"] == "arch":
                suffix, architecture = "-1-x86_64.pkg.tar.zst", "x86_64"
            elif target["kind"] == "deb":
                suffix, architecture = f"-1ubuntu{target['version']}_amd64.deb", "amd64"
            else:
                suffix, architecture = f"-1.fc{target['version']}.x86_64.rpm", "x86_64"
            row = next(row for row in rows if suffix in row)
            self.assertIn(target["distro"], row.lower())
            self.assertIn(target["version"], row.lower())
            self.assertIn(architecture, row)

    def test_generated_changelog_is_one_fixed_section(self):
        config = tomllib.loads((ROOT / "cliff.toml").read_text(encoding="utf-8-sig"))
        changelog = config["changelog"]
        self.assertEqual(re.findall(r"^## (.+)$", changelog["header"], re.MULTILINE), ["📜 Changelog"])
        self.assertIsNone(re.search(r"^\s*#{1,2} ", changelog["body"], re.MULTILINE))
        self.assertIn("#### {{ group | upper_first }}", changelog["body"])


if __name__ == "__main__":
    unittest.main()
