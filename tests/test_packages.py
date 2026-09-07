"""Offline packaging contracts; native builds/install tests run in distro CI jobs."""
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import tempfile
import tarfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def load_metadata():
    spec = importlib.util.spec_from_file_location("package_metadata", ROOT / "scripts/package_metadata.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PackageTest(unittest.TestCase):
    def setUp(self):
        self.metadata = load_metadata()
        self.temp = tempfile.TemporaryDirectory(prefix="kodometer-packages-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def test_packagers_refuse_arm_before_checkout_or_build_work(self):
        tools = self.directory / 'bin'
        tools.mkdir()
        marker = self.directory / 'git-was-called'
        (tools / 'uname').write_text('#!/bin/sh\nprintf "aarch64\\n"\n')
        (tools / 'git').write_text(f'#!/bin/sh\ntouch "{marker}"\nexit 99\n')
        for path in tools.iterdir():
            path.chmod(0o700)
        environment = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'])
        for script in ['package.sh', 'prepare-source.sh', 'build-native-package.sh',
                       'install-package-deps.sh', 'test-native-install.sh']:
            with self.subTest(script=script):
                marker.unlink(missing_ok=True)
                result = subprocess.run(['bash', str(ROOT / 'scripts' / script)], env=environment, capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(marker.exists())

    def test_x64_inventory_is_explicit(self):
        names = self.metadata.payloads("1.2.3")
        self.assertEqual(set(names), {
            "kodometer-1.2.3-source.tar.gz", "kodometer-1.2.3-aur.tar.gz",
            "kodometer-1.2.3-1-x86_64.pkg.tar.zst", "kodometer-1.2.3-linux-x86_64.tar.gz",
            "kodometer_1.2.3-1ubuntu26.04_amd64.deb",
            "kodometer-1.2.3-1.fc43.x86_64.rpm", "kodometer-1.2.3-1.fc44.x86_64.rpm",
        })
        for version in ["01.2.3", "1.2", "1.2.3/extra", "1.2.3;false"]:
            with self.assertRaises(ValueError):
                self.metadata.payloads(version)

    def fill(self):
        for name in self.metadata.payloads("1.2.3"):
            (self.directory / name).write_bytes(name.encode())

    def test_manifest_requires_complete_inventory(self):
        self.fill()
        missing = self.directory / self.metadata.payloads("1.2.3")[-1]
        missing.unlink()
        with self.assertRaises(ValueError):
            self.metadata.write_manifest(self.directory, "1.2.3")
        missing.write_bytes(b"restored")
        self.metadata.write_manifest(self.directory, "1.2.3")
        self.metadata.verify_manifest(self.directory, "1.2.3")
        missing.write_bytes(b"corrupted")
        with self.assertRaises(ValueError):
            self.metadata.verify_manifest(self.directory, "1.2.3")

    def test_manifest_rejects_paths_duplicates_and_missing_rows(self):
        self.fill()
        self.metadata.write_manifest(self.directory, "1.2.3")
        manifest = self.directory / self.metadata.manifest_name("1.2.3")
        original = manifest.read_text()
        for data in [original + original.splitlines()[0] + "\n", original.splitlines()[0] + "\n",
                     original + f"{'a' * 64}  ../unrelated\n", original.replace("  kodometer", "  /kodometer", 1)]:
            manifest.write_text(data)
            with self.assertRaises(ValueError):
                self.metadata.verify_manifest(self.directory, "1.2.3")
        manifest.write_text(original)
        self.metadata.verify_manifest(self.directory, "1.2.3")

    def test_manifest_rejects_symlinks_and_extra_payloads(self):
        self.fill()
        extra = self.directory / "unexpected.rpm"
        extra.write_text("not a release artifact")
        with self.assertRaises(ValueError):
            self.metadata.write_manifest(self.directory, "1.2.3")
        extra.unlink()
        name = self.metadata.payloads("1.2.3")[0]
        (self.directory / name).unlink()
        (self.directory / name).symlink_to(__file__)
        with self.assertRaises(ValueError):
            self.metadata.write_manifest(self.directory, "1.2.3")

    def test_aur_recipe_pins_source_and_architecture(self):
        checksum = hashlib.sha256(b"source archive").hexdigest()
        rendered = self.metadata.render_pkgbuild("1.2.3", checksum)
        self.assertIn("arch=('x86_64')", rendered)
        self.assertIn("pkgver=1.2.3", rendered)
        self.assertIn(checksum, rendered)
        self.assertIn("https://github.com/kyaulabs/kodometer/releases/download/v${pkgver}/kodometer-${pkgver}-source.tar.gz", rendered)
        self.assertNotIn("SKIP", rendered)
        self.assertNotIn("git+", rendered)
        for invalid in ["bad", "a" * 63, "A" * 64, "a" * 64 + "\n"]:
            with self.assertRaises(ValueError):
                self.metadata.render_pkgbuild("1.2.3", invalid)
        (self.directory / "PKGBUILD").write_text(rendered)
        result = subprocess.run(["bash", "-n", "PKGBUILD"], cwd=self.directory, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_aur_bundle_is_verified_before_extracting(self):
        self.fill()
        source = self.directory / "kodometer-1.2.3-source.tar.gz"
        recipe = self.metadata.render_pkgbuild("1.2.3", hashlib.sha256(source.read_bytes()).hexdigest())
        bundle = self.directory / "kodometer-1.2.3-aur.tar.gz"
        for bad in ["path", "duplicate", "recipe", "symlink", "oversized", None]:
            with self.subTest(bad=bad):
                with tarfile.open(bundle, "w:gz") as archive:
                    info = tarfile.TarInfo("../PKGBUILD" if bad == "path" else "PKGBUILD")
                    data = (recipe + "# altered" if bad == "recipe" else recipe).encode()
                    info.size = len(data)
                    if bad == "symlink":
                        info.type = tarfile.SYMTYPE
                        info.linkname = "../outside"
                        info.size = 0
                    if bad == "oversized":
                        data = b"x" * 65537
                        info.size = len(data)
                    archive.addfile(info, io.BytesIO(data))
                    if bad == "duplicate":
                        archive.addfile(info, io.BytesIO(data))
                    srcinfo = b"pkgbase = kodometer\n"
                    info = tarfile.TarInfo(".SRCINFO")
                    info.size = len(srcinfo)
                    archive.addfile(info, io.BytesIO(srcinfo))
                self.metadata.write_manifest(self.directory, "1.2.3")
                destination = self.directory.parent / (self.directory.name + "-recipes")
                if bad:
                    with self.assertRaises(ValueError):
                        self.metadata.prepare_aur(self.directory, "1.2.3", destination)
                    self.assertFalse(destination.exists())
                else:
                    self.metadata.prepare_aur(self.directory, "1.2.3", destination)
                    self.assertEqual((destination / "PKGBUILD").read_text(), recipe)
                    for path in destination.iterdir():
                        path.unlink()
                    destination.rmdir()

    def test_cpack_metadata_preserves_native_dependency_scanning(self):
        for target in ["ubuntu-26.04", "fedora-43", "fedora-44"]:
            directory = self.directory / target
            directory.mkdir()
            (directory / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.24)\nproject(kodometer VERSION 1.2.3 LANGUAGES NONE)\n'
                'set(KDE_INSTALL_DATADIR share)\n'
                f'set(KODOMETER_PACKAGE_TARGET "{target}")\n'
                f'include("{ROOT}/cmake/NativePackages.cmake")\n')
            result = subprocess.run(["cmake", "-S", str(directory), "-B", str(directory / "build")], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            config = (directory / "build/CPackConfig.cmake").read_text()
            if target.startswith("ubuntu"):
                self.assertIn('CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64"', config)
                self.assertIn('CPACK_DEBIAN_PACKAGE_SHLIBDEPS "ON"', config)
                self.assertIn('plasma-workspace (>= 4:6.4)', config)
            else:
                self.assertIn('CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64"', config)
                self.assertIn('CPACK_RPM_PACKAGE_AUTOREQ "ON"', config)
                self.assertIn('plasma-workspace >= 6.4', config)


if __name__ == "__main__":
    unittest.main()
