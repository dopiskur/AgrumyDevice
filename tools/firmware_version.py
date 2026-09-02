"""
Roadmap #94: derive the firmware version string at build time instead of hand-editing a constant
in main.cpp - a release's embedded version can then never disagree with its git tag.

Resolution order (first hit wins):
  1. FIRMWARE_VERSION environment variable  - what .github/workflows/release.yml sets from the
     pushed tag (v1.2.3 -> 1.2.3), and what a developer can set for a local release-style build.
  2. `git describe --tags --always --dirty`  - a local build gets e.g. "1.2.3-4-gabc1234-dirty"
     (semver-ish: the server treats anything unparseable as "older than any release", so a dev
     board is still offered the latest release - see AgrumyApi FirmwareVersion.IsNewer).
  3. "0.0.0-dev"                              - no git at all (tarball checkout).

Injected as -DFIRMWARE_VERSION="..." ; main.cpp falls back to "0.0.0-dev" if the define is absent
(e.g. a build system that skips extra_scripts), so the constant is never simply missing.
"""
import os
import subprocess

Import("env")  # noqa: F821 - PlatformIO SCons global


def _from_git():
    try:
        out = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=env.subst("$PROJECT_DIR"),  # noqa: F821
            stderr=subprocess.DEVNULL,
        )
        return out.decode().strip()
    except Exception:  # noqa: BLE001 - no git / not a checkout: fall through to the constant
        return ""


version = os.environ.get("FIRMWARE_VERSION", "").strip() or _from_git() or "0.0.0-dev"
if version.startswith("v"):
    version = version[1:]

# Quoted twice on purpose: the outer quotes survive the shell, the inner \" survive the
# preprocessor so the macro expands to a C string literal.
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", '\\"%s\\"' % version)])  # noqa: F821
print("firmware_version.py: FIRMWARE_VERSION =", version)
