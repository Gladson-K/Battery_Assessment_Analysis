# Neural Architecture Search (ML NAS)

This document covers the offline PyTorch neural architecture search pipeline. It
is separate from the **NAS telemetry platform** documented in
[architecture.md](architecture.md) and `../nas/README.md`.

## Purpose

The search pipeline evaluates compact tabular MLP architectures for the bench
6 V battery-safety dataset. It produces a reproducible best configuration and a
PyTorch checkpoint for experiments. The deployed firmware model remains the
constrained random forest documented in [model_card.md](model_card.md).

## Architecture

```text
battery_dataset_real_world_mixed.csv
        |
        v
Stratified 5-fold splits
        |
        v
random-search candidate sampler
  hidden layers: [32], [64], [32,16], [64,32], [64,32,16], [128,64]
  activation: relu / gelu / tanh
  dropout: 0.0 / 0.1 / 0.2 / 0.3
  learning rate: 1e-3 / 5e-4
  weight decay: 1e-4 / 1e-3
  batch size: 32 / 64
        |
        v
BatteryMLP training with per-fold StandardScaler
        |
        v
out-of-fold selection score
        |
        v
final full-data model + config + checkpoint
```

The selection score is:

```text
0.40 * ROC-AUC + 0.25 * recall + 0.20 * F1 + 0.15 * (1 - Brier)
```

Ties prefer fewer trainable parameters. Selection uses out-of-fold predictions,
so the held-out fold is not used for fitting or early stopping.

## Configuration

- Search config: `model/nas/config.yaml`
- Architecture: `model/nas/architecture.py`
- Search runner: `model/nas/search.py`
- Unit tests: `model/nas/test_architecture.py`

Run the search from the repository root:

```bash
python3 model/nas/search.py --config model/nas/config.yaml
```

## Selected model

The current run evaluated 24 candidates with seed 42 and selected `trial-009`:

| Field | Value |
| --- | --- |
| Model version | `nas-a54bf9f1` |
| Architecture | `64-32-16` ReLU MLP, dropout 0.3 |
| Parameters | 3,457 |
| Learning rate | 0.0005 |
| Weight decay | 0.001 |
| Batch size | 64 |
| Final epochs | 120 |
| Decision threshold | 0.50 |

Out-of-fold metrics:

| Metric | Value |
| --- | --- |
| Accuracy | 0.8781 |
| Recall (unsafe) | 0.8696 |
| Precision (unsafe) | 0.8861 |
| F1 | 0.8777 |
| ROC-AUC | 0.9644 |
| Brier | 0.0799 |

The existing random forest still has higher ROC-AUC and lower Brier score and
remains the firmware model. The NAS artifact is the best model within the MLP
search space and is tracked for reproducibility.

## Artifacts

| Artifact | Path | Git handling |
| --- | --- | --- |
| PyTorch checkpoint | `model/generated/nas_best_model.pt` | Git LFS |
| Best config | `model/generated/nas_best_config.json` | Git |
| Full search results | `model/generated/nas_search_results.json` | Git |

The checkpoint contains the state dict, architecture, normalization values,
feature order, class order, decision threshold, dataset SHA-256 and out-of-fold
metrics. `.gitattributes` routes `*.pt` files through Git LFS.
