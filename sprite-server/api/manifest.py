"""
manifest.py — Read, write, and version-bump /sprites/manifest.json.

The manifest schema:
  {
    "version": "0.1.3",           -- semver string, bumped on every publish
    "characters": ["muni", ...],  -- directories present under /sprites/
    "expressions": ["neutral", "happy", ...],  -- .sprite files found
    "updated": "2026-03-22"       -- ISO date of last publish
  }
"""

import json
import os
from datetime import date
from pathlib import Path


SPRITES_DIR = Path(os.environ.get("SPRITES_DIR", "/sprites"))
MANIFEST_PATH = SPRITES_DIR / "manifest.json"


def _default_manifest() -> dict:
    return {
        "version": "0.0.1",
        "characters": [],
        "expressions": [],
        "updated": str(date.today()),
    }


def load() -> dict:
    if not MANIFEST_PATH.exists():
        return _default_manifest()
    with MANIFEST_PATH.open() as f:
        return json.load(f)


def save(manifest: dict) -> None:
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    with MANIFEST_PATH.open("w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")


def _bump_patch(version: str) -> str:
    """Increment the patch component of a semver string."""
    parts = version.split(".")
    while len(parts) < 3:
        parts.append("0")
    parts[-1] = str(int(parts[-1]) + 1)
    return ".".join(parts)


def publish(character: str, expression: str) -> dict:
    """
    Record that character/expression.sprite has been published.
    Bumps the patch version and refreshes the updated date.
    Returns the updated manifest dict.
    """
    manifest = load()

    if character not in manifest["characters"]:
        manifest["characters"].append(character)
        manifest["characters"].sort()

    if expression not in manifest["expressions"]:
        manifest["expressions"].append(expression)
        manifest["expressions"].sort()

    manifest["version"] = _bump_patch(manifest["version"])
    manifest["updated"] = str(date.today())

    save(manifest)
    return manifest
