"""AUR updates use local bare repos and disposable signing keys, never the AUR."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AurTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.key_temp = tempfile.TemporaryDirectory(prefix="kodometer-test-signing-")
        cls.key_home = Path(cls.key_temp.name)
        cls.key_home.chmod(0o700)
        cls.key_env = dict(os.environ, GNUPGHOME=str(cls.key_home))
        cls.passphrase = "disposable-fixture-passphrase"
        cls.gpg("--quick-generate-key", "Kodometer fixture <fixture@example.invalid>", "ed25519", "sign", "0")
        records = cls.gpg("--with-colons", "--list-secret-keys").stdout
        cls.fingerprint = next(line.split(":")[9] for line in records.splitlines() if line.startswith("fpr:"))
        cls.secret = cls.gpg("--armor", "--export-secret-keys", cls.fingerprint).stdout

    @classmethod
    def gpg(cls, *args):
        return subprocess.run(["gpg", "--batch", "--pinentry-mode", "loopback", "--passphrase", cls.passphrase, *args], env=cls.key_env,
                              capture_output=True, text=True, check=True, timeout=30)

    @classmethod
    def tearDownClass(cls):
        subprocess.run(["gpgconf", "--kill", "gpg-agent"], env=cls.key_env, capture_output=True)
        cls.key_temp.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="kodometer-aur-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.remote = self.root / "aur.git"
        subprocess.run(["git", "init", "--quiet", "--bare", "--initial-branch=master", str(self.remote)], check=True)
        self.recipes = self.root / "recipes"
        self.recipes.mkdir()
        self.version("1.2.3")
        self.env = dict(self.key_env, AUR_SSH_PRIVATE_KEY="unused-by-local-file-transport",
                        AUR_SSH_KNOWN_HOSTS="aur.archlinux.org ssh-ed25519 fixture",
                        AUR_GPG_PRIVATE_KEY=self.secret, AUR_GPG_PASSPHRASE=self.passphrase,
                        AUR_SIGNING_KEY=self.fingerprint, AUR_COMMIT_NAME="Kodometer fixture",
                        AUR_COMMIT_EMAIL="fixture@example.invalid", GIT_TERMINAL_PROMPT="0",
                        GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0=f"url.{self.remote}.insteadOf",
                        GIT_CONFIG_VALUE_0="ssh://aur@aur.archlinux.org/kodometer.git")

    def version(self, version):
        self.current_version = version
        (self.recipes / "PKGBUILD").write_text(f"pkgname=kodometer\npkgver={version}\narch=('x86_64')\n")
        (self.recipes / ".SRCINFO").write_text(f"pkgbase = kodometer\n\tpkgver = {version}\n\tpkgrel = 1\n\tarch = x86_64\npkgname = kodometer\n")

    def update(self, success=True):
        result = subprocess.run(["bash", str(ROOT / "scripts/update-aur.sh"), str(self.recipes), self.current_version],
                                env=self.env, capture_output=True, text=True, timeout=40)
        self.assertNotIn(self.secret, result.stdout + result.stderr)
        self.assertNotIn(self.passphrase, result.stdout + result.stderr)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def head(self):
        result = subprocess.run(["git", "--git-dir", str(self.remote), "rev-parse", "--verify", "HEAD"], capture_output=True, text=True)
        return result.stdout.strip() if result.returncode == 0 else None

    def test_initial_update_is_signed_and_retry_is_noop(self):
        self.update()
        first = self.head()
        self.assertIsNotNone(first)
        result = subprocess.run(["git", "--git-dir", str(self.remote), "verify-commit", first], env=self.key_env, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.update()
        self.assertEqual(self.head(), first)
        self.version("1.2.4")
        self.update()
        self.assertNotEqual(self.head(), first)

    def test_old_release_never_downgrades_aur(self):
        self.version("2.0.0")
        self.update()
        latest = self.head()
        self.version("1.2.3")
        self.update()
        self.assertEqual(self.head(), latest)

    def test_same_version_edits_need_manual_review(self):
        self.update()
        first = self.head()
        with (self.recipes / "PKGBUILD").open("a") as stream:
            stream.write("# changed recipe\n")
        self.update(False)
        self.assertEqual(self.head(), first)

    def test_missing_credentials_bad_fingerprint_and_bad_passphrase_never_push(self):
        for key, value in [("AUR_SSH_PRIVATE_KEY", ""), ("AUR_SSH_KNOWN_HOSTS", ""),
                           ("AUR_GPG_PRIVATE_KEY", ""), ("AUR_SIGNING_KEY", "0" * 40),
                           ("AUR_GPG_PASSPHRASE", "wrong")]:
            with self.subTest(key=key):
                previous = self.env[key]
                self.env[key] = value
                self.update(False)
                self.assertIsNone(self.head())
                self.env[key] = previous

    def test_wrong_architecture_and_symlink_recipes_are_rejected(self):
        info = self.recipes / ".SRCINFO"
        info.write_text(info.read_text().replace("x86_64", "aarch64"))
        self.update(False)
        self.assertIsNone(self.head())
        self.version("1.2.3")
        (self.recipes / "PKGBUILD").unlink()
        (self.recipes / "PKGBUILD").symlink_to(ROOT / "README.md")
        self.update(False)
        self.assertIsNone(self.head())


if __name__ == "__main__":
    unittest.main()
