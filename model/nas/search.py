from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import random
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence

import numpy as np
import pandas as pd
import torch
import yaml
from sklearn.metrics import (
    accuracy_score,
    brier_score_loss,
    confusion_matrix,
    f1_score,
    precision_score,
    recall_score,
    roc_auc_score,
)
from sklearn.model_selection import StratifiedKFold
from sklearn.preprocessing import LabelEncoder, StandardScaler
from torch import nn
from torch.utils.data import DataLoader, TensorDataset

try:
    from .architecture import build_model, count_parameters
except ImportError:
    from architecture import build_model, count_parameters


ROOT = Path(__file__).resolve().parents[2]
SPACE_KEYS = (
    "hidden_layers",
    "activation",
    "dropout",
    "learning_rate",
    "weight_decay",
    "batch_size",
    "epochs",
)


@dataclass(frozen=True)
class Candidate:
    trial_id: str
    hidden_layers: tuple[int, ...]
    activation: str
    dropout: float
    learning_rate: float
    weight_decay: float
    batch_size: int
    epochs: int
    parameter_count: int

    def as_config(self) -> dict[str, Any]:
        return {
            "hidden_layers": list(self.hidden_layers),
            "activation": self.activation,
            "dropout": self.dropout,
            "learning_rate": self.learning_rate,
            "weight_decay": self.weight_decay,
            "batch_size": self.batch_size,
            "epochs": self.epochs,
        }


def resolve_path(path: str | Path) -> Path:
    candidate = Path(path)
    return candidate if candidate.is_absolute() else ROOT / candidate


def relative_path(path: str | Path) -> str:
    try:
        return str(Path(path).resolve().relative_to(ROOT))
    except ValueError:
        return str(Path(path).resolve())


def load_config(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        config = yaml.safe_load(handle)
    if not isinstance(config, dict):
        raise SystemExit("config root must be a mapping")
    for section in ("data", "search", "train", "output"):
        if section not in config or not isinstance(config[section], dict):
            raise SystemExit(f"missing config section: {section}")
    return config


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def set_determinism(seed: int) -> None:
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.set_num_threads(1)
    torch.use_deterministic_algorithms(True)


def sample_candidates(
    space: dict[str, Sequence[Any]],
    input_dim: int,
    trials: int,
    max_parameters: int,
    seed: int,
) -> list[Candidate]:
    for key in SPACE_KEYS:
        if key not in space or not space[key]:
            raise SystemExit(f"missing search space key: {key}")
    combinations = list(itertools.product(*(space[key] for key in SPACE_KEYS)))
    rng = np.random.default_rng(seed)
    indices = rng.permutation(len(combinations))[: min(trials, len(combinations))]
    candidates: list[Candidate] = []
    for index in indices:
        values = dict(zip(SPACE_KEYS, combinations[int(index)]))
        hidden_layers = tuple(int(width) for width in values["hidden_layers"])
        architecture = build_model(
            input_dim,
            hidden_layers,
            str(values["activation"]),
            float(values["dropout"]),
        )
        parameter_count = count_parameters(architecture)
        if parameter_count > max_parameters:
            continue
        candidates.append(
            Candidate(
                trial_id=f"trial-{len(candidates):03d}",
                hidden_layers=hidden_layers,
                activation=str(values["activation"]),
                dropout=float(values["dropout"]),
                learning_rate=float(values["learning_rate"]),
                weight_decay=float(values["weight_decay"]),
                batch_size=int(values["batch_size"]),
                epochs=int(values["epochs"]),
                parameter_count=parameter_count,
            )
        )
    if not candidates:
        raise SystemExit("search produced no candidates within the parameter budget")
    return candidates


def train_candidate(
    candidate: Candidate,
    features: np.ndarray,
    labels: np.ndarray,
    epochs: int,
    seed: int,
) -> tuple[nn.Module, StandardScaler]:
    scaler = StandardScaler()
    scaled = scaler.fit_transform(features).astype(np.float32)
    targets = labels.astype(np.float32)
    model = build_model(
        features.shape[1],
        candidate.hidden_layers,
        candidate.activation,
        candidate.dropout,
    )
    optimizer = torch.optim.AdamW(
        model.parameters(),
        lr=candidate.learning_rate,
        weight_decay=candidate.weight_decay,
    )
    positives = float(targets.sum())
    negatives = float(len(targets) - positives)
    pos_weight = torch.tensor([negatives / max(positives, 1.0)], dtype=torch.float32)
    criterion = nn.BCEWithLogitsLoss(pos_weight=pos_weight)
    dataset = TensorDataset(torch.from_numpy(scaled), torch.from_numpy(targets))
    generator = torch.Generator().manual_seed(seed)
    loader = DataLoader(
        dataset,
        batch_size=min(candidate.batch_size, len(dataset)),
        shuffle=True,
        generator=generator,
    )
    torch.manual_seed(seed)
    model.train()
    for _ in range(epochs):
        for batch_features, batch_labels in loader:
            optimizer.zero_grad(set_to_none=True)
            logits = model(batch_features)
            loss = criterion(logits, batch_labels)
            loss.backward()
            optimizer.step()
    model.eval()
    return model, scaler


def predict_probabilities(
    model: nn.Module,
    scaler: StandardScaler,
    features: np.ndarray,
) -> np.ndarray:
    scaled = scaler.transform(features).astype(np.float32)
    with torch.no_grad():
        logits = model(torch.from_numpy(scaled))
        probabilities = torch.sigmoid(logits).cpu().numpy()
    return probabilities.astype(np.float64)


def compute_metrics(
    labels: np.ndarray,
    probabilities: np.ndarray,
    threshold: float,
) -> dict[str, Any]:
    predictions = (probabilities >= threshold).astype(np.int64)
    if len(np.unique(labels)) < 2:
        roc_auc = 0.5
    else:
        roc_auc = float(roc_auc_score(labels, probabilities))
    return {
        "accuracy": float(accuracy_score(labels, predictions)),
        "precision": float(precision_score(labels, predictions, zero_division=0)),
        "recall": float(recall_score(labels, predictions, zero_division=0)),
        "f1": float(f1_score(labels, predictions, zero_division=0)),
        "roc_auc": roc_auc,
        "brier": float(brier_score_loss(labels, probabilities)),
        "confusion_matrix": confusion_matrix(labels, predictions).tolist(),
        "threshold": threshold,
    }


def selection_score(metrics: dict[str, Any], weights: dict[str, Any]) -> float:
    return (
        float(weights["roc_auc"]) * float(metrics["roc_auc"])
        + float(weights["recall"]) * float(metrics["recall"])
        + float(weights["f1"]) * float(metrics["f1"])
        + float(weights["brier"]) * (1.0 - float(metrics["brier"]))
    )


def evaluate_candidate(
    candidate: Candidate,
    features: np.ndarray,
    labels: np.ndarray,
    folds: int,
    threshold: float,
    weights: dict[str, Any],
    seed: int,
) -> dict[str, Any]:
    splitter = StratifiedKFold(n_splits=folds, shuffle=True, random_state=seed)
    out_of_fold = np.zeros(len(labels), dtype=np.float64)
    fold_auc: list[float] = []
    for fold, (train_index, validation_index) in enumerate(
        splitter.split(features, labels)
    ):
        model, scaler = train_candidate(
            candidate,
            features[train_index],
            labels[train_index],
            candidate.epochs,
            seed + fold,
        )
        probabilities = predict_probabilities(
            model, scaler, features[validation_index]
        )
        out_of_fold[validation_index] = probabilities
        validation_labels = labels[validation_index]
        if len(np.unique(validation_labels)) == 2:
            fold_auc.append(float(roc_auc_score(validation_labels, probabilities)))
    metrics = compute_metrics(labels, out_of_fold, threshold)
    metrics["fold_roc_auc_mean"] = float(np.mean(fold_auc)) if fold_auc else 0.5
    metrics["fold_roc_auc_std"] = (
        float(np.std(fold_auc)) if fold_auc else 0.0
    )
    return {
        "trial_id": candidate.trial_id,
        "config": candidate.as_config(),
        "parameter_count": candidate.parameter_count,
        "metrics": metrics,
        "selection_score": selection_score(metrics, weights),
    }


def model_version(
    candidate: Candidate,
    dataset_sha256: str,
    seed: int,
    folds: int,
) -> str:
    payload = json.dumps(
        {
            "config": candidate.as_config(),
            "dataset_sha256": dataset_sha256,
            "seed": seed,
            "folds": folds,
        },
        sort_keys=True,
    ).encode("utf-8")
    return f"nas-{hashlib.sha256(payload).hexdigest()[:8]}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default="model/nas/config.yaml")
    parser.add_argument("--trials", type=int, default=None)
    parser.add_argument("--seed", type=int, default=None)
    args = parser.parse_args()

    config_path = resolve_path(args.config)
    config = load_config(config_path)
    data_config = config["data"]
    search_config = config["search"]
    train_config = config["train"]
    output_config = config["output"]

    seed = int(args.seed if args.seed is not None else search_config["random_state"])
    set_determinism(seed)

    data_path = resolve_path(data_config["path"])
    features = list(data_config["features"])
    label_column = str(data_config["label"])
    expected_classes = list(data_config["classes"])
    data = pd.read_csv(data_path)
    missing = [column for column in features + [label_column] if column not in data.columns]
    if missing:
        raise SystemExit(f"missing columns: {missing}")
    if data[features + [label_column]].isna().any().any():
        raise SystemExit("dataset contains missing values")

    label_encoder = LabelEncoder()
    labels = label_encoder.fit_transform(data[label_column].to_numpy())
    if list(label_encoder.classes_) != expected_classes:
        raise SystemExit(f"unexpected label classes: {list(label_encoder.classes_)}")
    feature_matrix = data[features].to_numpy(dtype=np.float64)
    dataset_sha256 = file_sha256(data_path)

    trials = int(args.trials if args.trials is not None else search_config["trials"])
    candidates = sample_candidates(
        search_config["space"],
        feature_matrix.shape[1],
        trials,
        int(search_config["max_parameters"]),
        seed,
    )
    folds = int(search_config["folds"])
    threshold = float(train_config["decision_threshold"])
    weights = search_config["selection_weights"]

    results = []
    for candidate in candidates:
        result = evaluate_candidate(
            candidate,
            feature_matrix,
            labels,
            folds,
            threshold,
            weights,
            seed,
        )
        results.append(result)
        metrics = result["metrics"]
        print(
            f"{candidate.trial_id} layers={list(candidate.hidden_layers)} "
            f"act={candidate.activation} dropout={candidate.dropout:.2f} "
            f"params={candidate.parameter_count} "
            f"auc={metrics['roc_auc']:.4f} recall={metrics['recall']:.4f} "
            f"f1={metrics['f1']:.4f} brier={metrics['brier']:.4f} "
            f"score={result['selection_score']:.4f}"
        )

    results.sort(
        key=lambda result: (
            -float(result["selection_score"]),
            int(result["parameter_count"]),
            result["trial_id"],
        )
    )
    best_result = results[0]
    best_values = best_result["config"]
    best_candidate = Candidate(
        trial_id=best_result["trial_id"],
        hidden_layers=tuple(int(width) for width in best_values["hidden_layers"]),
        activation=str(best_values["activation"]),
        dropout=float(best_values["dropout"]),
        learning_rate=float(best_values["learning_rate"]),
        weight_decay=float(best_values["weight_decay"]),
        batch_size=int(best_values["batch_size"]),
        epochs=int(best_values["epochs"]),
        parameter_count=int(best_result["parameter_count"]),
    )
    version = model_version(best_candidate, dataset_sha256, seed, folds)
    final_epochs = int(train_config["final_epochs"])
    final_model, final_scaler = train_candidate(
        best_candidate,
        feature_matrix,
        labels,
        final_epochs,
        seed + 10000,
    )

    checkpoint_path = resolve_path(output_config["checkpoint"])
    best_config_path = resolve_path(output_config["best_config"])
    results_path = resolve_path(output_config["search_results"])
    for path in (checkpoint_path, best_config_path, results_path):
        path.parent.mkdir(parents=True, exist_ok=True)

    checkpoint = {
        "format_version": 1,
        "model_name": "battery-nas-mlp",
        "model_version": version,
        "architecture": {
            "hidden_layers": list(best_candidate.hidden_layers),
            "activation": best_candidate.activation,
            "dropout": best_candidate.dropout,
        },
        "training": {
            "learning_rate": best_candidate.learning_rate,
            "weight_decay": best_candidate.weight_decay,
            "batch_size": best_candidate.batch_size,
            "epochs": final_epochs,
            "seed": seed,
        },
        "normalization": {
            "mean": final_scaler.mean_.astype(float).tolist(),
            "scale": final_scaler.scale_.astype(float).tolist(),
        },
        "features": features,
        "classes": expected_classes,
        "positive_class_index": int(list(label_encoder.classes_).index(expected_classes[1])),
        "decision_threshold": threshold,
        "state_dict": final_model.state_dict(),
        "metrics_oof": best_result["metrics"],
        "selection_score": best_result["selection_score"],
        "parameter_count": best_candidate.parameter_count,
        "dataset_sha256": dataset_sha256,
        "search_trials": len(candidates),
    }
    torch.save(checkpoint, checkpoint_path)

    best_payload = {
        "model_name": checkpoint["model_name"],
        "model_version": version,
        "checkpoint": relative_path(checkpoint_path),
        "search_config": relative_path(config_path),
        "dataset": {
            "path": relative_path(data_path),
            "sha256": dataset_sha256,
            "rows": int(len(data)),
        },
        "features": features,
        "classes": expected_classes,
        "decision_threshold": threshold,
        "architecture": checkpoint["architecture"],
        "training": checkpoint["training"],
        "parameter_count": best_candidate.parameter_count,
        "selection_weights": weights,
        "selection_score": best_result["selection_score"],
        "metrics_oof": best_result["metrics"],
        "search_trials": len(candidates),
        "random_state": seed,
        "folds": folds,
    }
    best_config_path.write_text(
        json.dumps(best_payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    results_payload = {
        "model_version": version,
        "best_trial_id": best_candidate.trial_id,
        "selection_weights": weights,
        "folds": folds,
        "random_state": seed,
        "trials": results,
    }
    results_path.write_text(
        json.dumps(results_payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    print(f"best trial: {best_candidate.trial_id}")
    print(f"model version: {version}")
    print(f"checkpoint: {checkpoint_path}")
    print(f"best config: {best_config_path}")
    print(f"search results: {results_path}")


if __name__ == "__main__":
    main()
