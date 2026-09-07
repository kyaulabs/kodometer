"""Run with python3 -m unittest discover -s tests -p test_release.py.

All release mutations use offline command fixtures, never the real git or gh.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import textwrap
import unittest

REPO = Path(__file__).resolve().parents[1]


def workflow_step(name):
    lines = (REPO / ".github/workflows/release.yml").read_text().splitlines()
    start = next(i for i, line in enumerate(lines) if line.strip() == f"- name: {name}")
    for i in range(start + 1, len(lines)):
        if lines[i].strip().startswith("run:"):
            value = lines[i].split("run:", 1)[1].strip()
            if value != "|":
                return value
            end = i + 1
            while end < len(lines) and (not lines[end].strip() or lines[end].startswith("          ")):
                end += 1
            return textwrap.dedent("\n".join(lines[i + 1:end]))
    raise AssertionError("step has no shell body")


class ReleaseTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="kodometer-release-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ["bin", "scripts", "applet", "docs/releases", "dist"]:
            (self.root / name).mkdir(parents=True, exist_ok=True)
        for script in ["validate-release.sh", "publish-release.sh"]:
            source = REPO / "scripts" / script
            if source.exists():
                shutil.copyfile(source, self.root / "scripts" / script)
                (self.root / "scripts" / script).chmod(0o755)
        fixture = self.root / "bin/fixture"
        shutil.copyfile(REPO / "tests/release_command_fixture.py", fixture)
        fixture.chmod(0o755)
        for command in ["git", "gh", "git-cliff"]:
            (self.root / "bin" / command).symlink_to(fixture)
        self.env = dict(os.environ, PATH=f"{self.root / 'bin'}:/usr/bin:/bin",
                        RELEASE_TEST_ROOT=str(self.root), GH_TOKEN="fixture-workflow-token",
                        BOT_TOKEN="fixture-bot-token", GITHUB_REPOSITORY="fixture/kodometer",
                        RELEASE_COMMIT="a" * 40, RELEASE_BRANCH="release/0.1.0",
                        GITHUB_OUTPUT=str(self.root / "output"))
        self.versions("0.1.0")
        (self.root / "docs/releases/0.1.0.md").write_text("Release overview\n\n")
        self.artifact = f"kodometer-0.1.0-linux-{os.uname().machine}.tar.gz"
        self.write_assets(b"fixture archive")
        self.set_state({})

    def versions(self, value):
        (self.root / "CMakeLists.txt").write_text(f"project(kodometer\n VERSION {value}\n)\n")
        (self.root / "applet/metadata.json").write_text(json.dumps({"KPlugin": {"Version": value}}))
        (self.root / "package.json").write_text(json.dumps({"version": value}))

    def write_assets(self, contents):
        (self.root / "dist" / self.artifact).write_bytes(contents)
        digest = hashlib.sha256(contents).hexdigest()
        (self.root / "dist" / (self.artifact + ".sha256")).write_text(f"{digest}  {self.artifact}\n")

    def state(self):
        return json.loads((self.root / "state.json").read_text())

    def set_state(self, state):
        (self.root / "state.json").write_text(json.dumps(state))

    def run_shell(self, body):
        result = subprocess.run(["bash", "-euo", "pipefail", "-c", body], cwd=self.root,
                                env=self.env, capture_output=True, text=True, timeout=20)
        self.assertNotIn("fixture-workflow-token", result.stdout + result.stderr)
        self.assertNotIn("fixture-bot-token", result.stdout + result.stderr)
        return result

    def publish(self, succeeds=True):
        result = self.run_shell('scripts/publish-release.sh "$RELEASE_BRANCH"')
        if succeeds:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return self.state()

    def mutations(self, state=None):
        events = (state or self.state()).get("events", [])
        return [event for event in events if event[:2] in [["git", "tag"], ["git", "push"]]
                or event[:3] in [["gh", "release", action] for action in ["create", "upload", "edit"]]
                or event[:3] == ["gh", "pr", "create"]]

    def test_version_validation_and_workflow_output(self):
        result = self.run_shell(workflow_step("Validate release version"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.root / "output").read_text(), "version=0.1.0\n")

    def test_workflow_does_not_hide_failed_validation(self):
        self.env["RELEASE_BRANCH"] = "release/9.9.9"
        result = self.run_shell(workflow_step("Validate release version"))
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.root / "output").exists() and (self.root / "output").read_text())

    def test_version_mismatches(self):
        for filename, contents in [
            ("CMakeLists.txt", "project(kodometer\n VERSION 0.2.0\n)\n"),
            ("applet/metadata.json", '{"KPlugin":{"Version":"0.2.0"}}'),
            ("package.json", '{"version":"0.2.0"}'),
            ("applet/metadata.json", "not JSON"),
        ]:
            with self.subTest(filename=filename, contents=contents):
                self.versions("0.1.0")
                (self.root / filename).write_text(contents)
                self.assertNotEqual(self.run_shell('scripts/validate-release.sh release/0.1.0').returncode, 0)

    def test_canonical_release_branches(self):
        for value in ["01.1.0", "0.01.0", "0.1.00"]:
            with self.subTest(value=value):
                self.versions(value)
                self.assertNotEqual(self.run_shell(f'scripts/validate-release.sh release/{value}').returncode, 0)
        self.versions("0.1.0")
        for branch in ["", "develop", "release/0.1", "release/0.1.0-rc1", "release/0.1.0/path"]:
            with self.subTest(branch=branch):
                self.env["GITHUB_HEAD_REF"] = branch
                self.assertNotEqual(self.run_shell('scripts/validate-release.sh').returncode, 0)

    def test_new_release_is_verified_as_draft_before_publication(self):
        state = self.publish()
        self.assertEqual(state["release"], "published")
        self.assertEqual(state["pr"], "42")
        actions = [event[:3] for event in state["events"]]
        self.assertLess(actions.index(["gh", "release", "upload"]), actions.index(["gh", "release", "download"]))
        self.assertLess(actions.index(["gh", "release", "download"]), actions.index(["gh", "release", "edit"]))
        self.assertLess(actions.index(["gh", "release", "edit"]), actions.index(["gh", "pr", "create"]))

    def test_preflight_failures_make_no_mutations(self):
        for variable, value in [("BOT_TOKEN", ""), ("GH_TOKEN", ""), ("RELEASE_COMMIT", "b" * 40),
                                ("GITHUB_REPOSITORY", ""), ("GITHUB_REPOSITORY", "bad/path/extra")]:
            with self.subTest(variable=variable):
                original = self.env[variable]
                self.env[variable] = value
                self.set_state({})
                self.publish(False)
                self.assertEqual(self.mutations(), [])
                self.env[variable] = original
        for flag in ["dirty", "fail_bot", "fail_api", "fail_notes"]:
            with self.subTest(flag=flag):
                self.set_state({flag: True})
                self.publish(False)
                self.assertEqual(self.mutations(), [])

    def test_bad_archive_or_checksum_stops_before_tagging(self):
        checksum = self.root / "dist" / (self.artifact + ".sha256")
        for value in ["incorrect", f"{'0' * 64}  ../../unrelated\n"]:
            with self.subTest(value=value):
                checksum.write_text(value)
                self.set_state({})
                self.publish(False)
                self.assertEqual(self.mutations(), [])
        checksum.unlink()
        self.publish(False)
        self.assertEqual(self.mutations(), [])
        self.write_assets(b"fixture archive")
        (self.root / "dist" / self.artifact).unlink()
        self.publish(False)
        self.assertEqual(self.mutations(), [])

    def test_wrong_or_lightweight_tag_is_rejected(self):
        for state in [{"tag": "b" * 40}, {"tag": "a" * 40, "tag_type": "commit"},
                      {"release": "published"}, {"release": "prerelease"}]:
            with self.subTest(state=state):
                self.set_state(state)
                self.publish(False)
                self.assertEqual(self.mutations(), [])

    def test_interrupted_upload_resumes_without_duplicate_release(self):
        self.set_state({"fail_upload_once": True})
        state = self.publish(False)
        self.assertEqual(state["release"], "draft")
        self.assertNotIn("pr", state)
        state = self.publish()
        self.assertEqual(state["release"], "published")
        self.assertEqual(sum(event[:3] == ["gh", "release", "create"] for event in state["events"]), 1)

    def test_transport_failures_never_publish_unverified_assets(self):
        for flag in ["fail_push", "fail_create", "fail_download", "corrupt_download", "different_download_pair", "fail_publish"]:
            with self.subTest(flag=flag):
                self.set_state({flag: True})
                state = self.publish(False)
                self.assertNotEqual(state.get("release"), "published")
                self.assertNotIn("pr", state)

    def test_published_release_is_immutable_even_after_rebuild(self):
        state = self.publish()
        original = (self.root / "remote" / self.artifact).read_bytes()
        state["events"] = []
        self.set_state(state)
        self.write_assets(b"different rebuild from a newer toolchain")
        state = self.publish()
        self.assertEqual(self.mutations(state), [])
        self.assertEqual((self.root / "remote" / self.artifact).read_bytes(), original)

    def test_missing_or_corrupted_published_assets_are_not_overwritten(self):
        state = self.publish()
        state["events"] = []
        self.set_state(state)
        checksum = self.root / "remote" / (self.artifact + ".sha256")
        checksum.write_text("invalid checksum")
        self.publish(False)
        self.assertEqual(self.mutations(), [])
        checksum.unlink()
        self.publish(False)
        self.assertEqual(self.mutations(), [])

    def test_backmerge_failure_can_retry_without_republication(self):
        self.set_state({"fail_backmerge": True})
        state = self.publish(False)
        self.assertEqual(state["release"], "published")
        state.update(fail_backmerge=False, events=[])
        self.set_state(state)
        state = self.publish()
        self.assertEqual([event[:3] for event in self.mutations(state)], [["gh", "pr", "create"]])

    def test_failed_tag_push_can_resume(self):
        self.set_state({"fail_push": True})
        state = self.publish(False)
        self.assertEqual(state["tag"], "a" * 40)
        self.assertNotIn("remote_tag", state)
        state["fail_push"] = False
        self.set_state(state)
        self.assertEqual(self.publish()["release"], "published")

    def test_existing_backmerge_is_reused_on_initial_publication(self):
        self.set_state({"pr": "123"})
        state = self.publish()
        self.assertEqual(state["pr"], "123")
        self.assertFalse(any(event[:3] == ["gh", "pr", "create"] for event in state["events"]))

    def test_concurrently_published_draft_is_not_uploaded_again(self):
        self.set_state({"publish_race": True})
        state = self.publish(False)
        self.assertFalse(any(event[:3] == ["gh", "release", "upload"] for event in state["events"]))


if __name__ == "__main__":
    unittest.main()
