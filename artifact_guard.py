"""Integrity-checked loading of trusted local model artefacts."""

from __future__ import annotations

import hashlib
from pathlib import Path
from typing import Any

import joblib


ARTIFACT_SHA256 = {
    "battery_model_real_world.pkl": "9d435df9839dca8772d9f3c447380b8dc190d3a3834c8a314a51938e7c426c9c",
    "model/generated/battery_model_v2.pkl": "b1e54323920b23764b9922c130cd7c1826f8a0d157aa4327ea28ca78bf0c8a13",
}


def sha256(path: str | Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_verified(path: str | Path, expected: str | None = None) -> Any:
    file = Path(path)
    key = str(file).replace("\\", "/")
    pinned = expected or ARTIFACT_SHA256.get(key)
    if pinned is None:
        raise SystemExit(f"no pinned SHA-256 for {key}; refusing to load")
    actual = sha256(file)
    if actual != pinned:
        raise SystemExit(f"SHA-256 mismatch for {key}: expected {pinned}, got {actual}")
    return joblib.load(file)
