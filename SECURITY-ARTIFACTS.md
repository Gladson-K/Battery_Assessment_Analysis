# Model artefact integrity — batteryproject

The runtime loads `battery_model_real_world.pkl` through `artifact_guard.load_verified`, which pins the file to a SHA-256 recorded in `artifact_guard.py` and refuses to load a changed or unknown file. This limits exposure to pickle/joblib deserialisation and accidental artefact swaps.

## Pinned artefacts

| Artefact | SHA-256 |
| --- | --- |
| `battery_model_real_world.pkl` | `9d435df9839dca8772d9f3c447380b8dc190d3a3834c8a314a51938e7c426c9c` |
| `model/generated/battery_model_v2.pkl` | `b1e54323920b23764b9922c130cd7c1826f8a0d157aa4327ea28ca78bf0c8a13` |

## Rules

1. Never load a `.pkl`, `.joblib`, or `.pth` file from an untrusted source, a download, or a chat attachment.
2. If a model is retrained, regenerate the hash with `sha256sum`, review the diff, and update `ARTIFACT_SHA256` in the same change. An unreviewed hash update is a release blocker.
3. The ML classifier is advisory and bench-only. The deterministic rule engine and hardware protection path remain authoritative; see `docs/safety_case.md` and `docs/model_card.md`.
4. Target format for new artefacts: ONNX or safetensors with a signed checksum manifest. Do not add new pickle-based artefacts.
5. The existing pickle was produced with scikit-learn 1.3.2 and is known to emit version warnings under newer versions. Treat it as legacy: retrain and migrate rather than silently upgrading the loader.
