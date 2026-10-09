"""DamnationCE's version, for the builds (tools/linux_build.py,
windows_build.py, macos_build.py; port/android/app/build.gradle reads the
same):

  - VERSION, in the repository's root, is the version being made.
  - A release is built by GitHub Actions from its tag, v<VERSION>
    (tools/ci_build.py gives HALO_VERSION, and HALO_RELEASE_BUILD=1, the
    only builds whose self-updater looks for newer releases).
  - nightly.yml's builds are <VERSION>-nightly.<date> (HALO_VERSION, not a
    release's); a push to main's are <VERSION>+<commit>; any other build
    is <VERSION>-dev.
"""

import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def base_version() -> str:
    """VERSION's"""
    return (ROOT / "VERSION").read_text(encoding="utf-8").strip()


def build_commit() -> str:
    """the commit a CI build was made from (shortened), or "" """
    return os.environ.get("HALO_BUILD_COMMIT", "")[:7]


def update_channel() -> str:
    """"latest" for a build of main pushed to GitHub, whose updater offers
    each newer push (build.yml publishes them as the "latest" pre-release);
    "" for any other build"""
    return "latest" if os.environ.get("HALO_UPDATE_CHANNEL") == "latest" and build_commit() else ""


def version() -> str:
    """this build's"""
    if os.environ.get("HALO_VERSION"):
        return os.environ["HALO_VERSION"]
    if update_channel():
        return f"{base_version()}+{build_commit()}"
    return f"{base_version()}-dev"


def release_build() -> bool:
    """whether this build is a release's (and so looks for newer ones)"""
    return os.environ.get("HALO_RELEASE_BUILD") == "1"
