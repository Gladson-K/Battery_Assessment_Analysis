#!/usr/bin/env python3
import argparse
import json
import math
import zlib
from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import (
    accuracy_score,
    brier_score_loss,
    confusion_matrix,
    f1_score,
    precision_score,
    recall_score,
    roc_auc_score,
)
from sklearn.model_selection import StratifiedKFold, cross_val_score, train_test_split
from sklearn.pipeline import make_pipeline
from sklearn.preprocessing import LabelEncoder, StandardScaler
from sklearn.tree import DecisionTreeClassifier

FEATURES = [
    "T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8",
    "Voltage", "Current", "Rain_A", "Rain_D",
]
FEATURE_COUNT = len(FEATURES)


def build_models():
    return {
        "logistic_regression": make_pipeline(
            StandardScaler(),
            LogisticRegression(max_iter=2000, class_weight="balanced", random_state=42),
        ),
        "decision_tree": make_pipeline(
            StandardScaler(),
            DecisionTreeClassifier(max_depth=6, min_samples_leaf=3, class_weight="balanced",
                                   random_state=42),
        ),
        "random_forest": make_pipeline(
            StandardScaler(),
            RandomForestClassifier(
                n_estimators=24,
                max_depth=8,
                min_samples_leaf=2,
                class_weight="balanced_subsample",
                random_state=42,
                n_jobs=1,
            ),
        ),
    }


def evaluate_model(model, x_train, x_test, y_train, y_test):
    model.fit(x_train, y_train)
    predictions = model.predict(x_test)
    probabilities = model.predict_proba(x_test)[:, 1]
    metrics = {
        "accuracy": float(accuracy_score(y_test, predictions)),
        "precision": float(precision_score(y_test, predictions, zero_division=0)),
        "recall": float(recall_score(y_test, predictions, zero_division=0)),
        "f1": float(f1_score(y_test, predictions, zero_division=0)),
        "roc_auc": float(roc_auc_score(y_test, probabilities)),
        "brier": float(brier_score_loss(y_test, probabilities)),
        "confusion_matrix": confusion_matrix(y_test, predictions).tolist(),
    }
    return metrics


def node_probability(tree, node_id):
    value = np.asarray(tree.value[node_id], dtype=np.float64).reshape(-1)
    total = float(value.sum())
    if total <= 0.0:
        return 0.5
    if value.size < 2:
        return 0.0
    return float(value[1] / total)


def export_tree(tree, scaler, next_global_index):
    nodes = []

    def visit(node_id):
        local_index = len(nodes)
        nodes.append(None)
        left = int(tree.children_left[node_id])
        right = int(tree.children_right[node_id])
        if left == -1 or right == -1:
            nodes[local_index] = {
                "threshold": 0.0,
                "leaf_p": node_probability(tree, node_id),
                "left": -1,
                "right": -1,
                "feature": 0,
                "flags": 1,
            }
            return local_index
        feature = int(tree.feature[node_id])
        threshold_scaled = float(tree.threshold[node_id])
        mean = float(scaler.mean_[feature])
        scale = float(scaler.scale_[feature])
        if not math.isfinite(scale) or scale <= 0.0:
            scale = 1.0
        threshold_raw = mean + threshold_scaled * scale
        left_index = visit(left)
        right_index = visit(right)
        nodes[local_index] = {
            "threshold": threshold_raw,
            "leaf_p": 0.0,
            "left": left_index,
            "right": right_index,
            "feature": feature,
            "flags": 0,
        }
        return local_index

    root_local = visit(0)
    if root_local != 0:
        raise RuntimeError("tree root must be index 0")
    return nodes


def domain_bounds(x_raw, feature_names):
    bounds = []
    for index, name in enumerate(feature_names):
        column = np.asarray(x_raw[:, index], dtype=np.float64)
        low = float(column.min())
        high = float(column.max())
        span = high - low
        if name.startswith("T"):
            margin = max(0.10 * span, 5.0)
        elif name in ("Voltage", "Current"):
            margin = max(0.10 * span, 0.5)
        elif name == "Rain_A":
            margin = max(0.10 * span, 50.0)
        else:
            margin = max(0.10 * span, 0.1)
        bounds.append((low - margin, high + margin))
    return bounds


def c_float(value):
    if not math.isfinite(value):
        return "0.0f"
    return f"{value:.8f}f"


def generate_header(model, scaler, bounds, version, checksum, feature_names):
    all_nodes = []
    roots = []
    sizes = []
    for estimator in model.estimators_:
        tree = estimator.tree_
        nodes = export_tree(tree, scaler, len(all_nodes))
        roots.append(len(all_nodes))
        sizes.append(len(nodes))
        all_nodes.extend(nodes)

    lines = []
    lines.append("#ifndef BATTERY_RF_MODEL_H")
    lines.append("#define BATTERY_RF_MODEL_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"#define RF_FEATURE_COUNT {FEATURE_COUNT}")
    lines.append(f"#define RF_TREE_COUNT {len(roots)}")
    lines.append(f"#define RF_NODE_COUNT {len(all_nodes)}")
    lines.append(f"#define RF_MODEL_VERSION \"{version}\"")
    lines.append(f"#define RF_MODEL_CHECKSUM 0x{checksum:08X}u")
    lines.append("#define RF_NODE_FLAG_LEAF 0x01u")
    lines.append("")
    lines.append("typedef struct {")
    lines.append("    float threshold;")
    lines.append("    float leaf_p;")
    lines.append("    int16_t left;")
    lines.append("    int16_t right;")
    lines.append("    uint8_t feature;")
    lines.append("    uint8_t flags;")
    lines.append("} rf_node_t;")
    lines.append("")
    lines.append("static const rf_node_t RF_NODES[RF_NODE_COUNT] = {")
    for node in all_nodes:
        lines.append(
            "    {"
            f"{c_float(node['threshold'])}, {c_float(node['leaf_p'])}, "
            f"{node['left']}, {node['right']}, {node['feature']}, {node['flags']}"
            "},"
        )
    lines.append("};")
    lines.append("")
    lines.append("static const int16_t RF_TREE_ROOTS[RF_TREE_COUNT] = {")
    lines.append("    " + ", ".join(str(root) for root in roots) + ",")
    lines.append("};")
    lines.append("")
    lines.append("static const uint16_t RF_TREE_SIZES[RF_TREE_COUNT] = {")
    lines.append("    " + ", ".join(str(size) for size in sizes) + ",")
    lines.append("};")
    lines.append("")
    lines.append("static const float RF_DOMAIN_MIN[RF_FEATURE_COUNT] = {")
    lines.append("    " + ", ".join(c_float(low) for low, _ in bounds) + ",")
    lines.append("};")
    lines.append("")
    lines.append("static const float RF_DOMAIN_MAX[RF_FEATURE_COUNT] = {")
    lines.append("    " + ", ".join(c_float(high) for _, high in bounds) + ",")
    lines.append("};")
    lines.append("")
    lines.append(f"static const char *const RF_FEATURE_NAMES[RF_FEATURE_COUNT] = {{")
    lines.append("    " + ", ".join(f'"{name}"' for name in feature_names) + ",")
    lines.append("};")
    lines.append("")
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines), roots, sizes, all_nodes


def generate_test_vectors(x_raw, probabilities, labels, count=80):
    rng = np.random.default_rng(42)
    indices = np.arange(len(x_raw))
    if count < len(indices):
        indices = rng.choice(indices, size=count, replace=False)
    lines = []
    lines.append("#ifndef BATTERY_MODEL_TEST_VECTORS_H")
    lines.append("#define BATTERY_MODEL_TEST_VECTORS_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"#define MODEL_TEST_VECTOR_COUNT {len(indices)}")
    lines.append("")
    lines.append("typedef struct {")
    lines.append(f"    float x[{FEATURE_COUNT}];")
    lines.append("    float expected_p_unsafe;")
    lines.append("    uint8_t expected_label;")
    lines.append("} model_test_vector_t;")
    lines.append("")
    lines.append("static const model_test_vector_t MODEL_TEST_VECTORS[MODEL_TEST_VECTOR_COUNT] = {")
    for index in indices:
        row = x_raw[index]
        values = ", ".join(c_float(float(value)) for value in row)
        lines.append(
            f"    {{{{{values}}}, {c_float(float(probabilities[index]))}, "
            f"{int(labels[index])}}},"
        )
    lines.append("};")
    lines.append("")
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default="battery_dataset_real_world_mixed.csv")
    parser.add_argument("--header", default="firmware/ml/rf_model.h")
    parser.add_argument("--header-copy", default="model/generated/rf_model.h")
    parser.add_argument("--vectors", default="firmware/tests/model_test_vectors.h")
    parser.add_argument("--metrics", default="model/generated/metrics.json")
    parser.add_argument("--model-out", default="model/generated/battery_model_v2.pkl")
    parser.add_argument("--test-size", type=float, default=0.30)
    args = parser.parse_args()

    data = pd.read_csv(args.data)
    missing = [column for column in FEATURES + ["Label"] if column not in data.columns]
    if missing:
        raise SystemExit(f"missing columns: {missing}")
    if data[FEATURES + ["Label"]].isna().any().any():
        raise SystemExit("dataset contains missing values")

    x_raw = data[FEATURES].to_numpy(dtype=np.float64)
    label_encoder = LabelEncoder()
    y = label_encoder.fit_transform(data["Label"].to_numpy())
    if list(label_encoder.classes_) != ["Safe", "Unsafe"]:
        raise SystemExit(f"unexpected label order: {list(label_encoder.classes_)}")

    x_train, x_test, y_train, y_test = train_test_split(
        x_raw, y, test_size=args.test_size, random_state=42, stratify=y
    )

    models = build_models()
    results = {}
    fitted = {}
    cv = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
    for name, model in models.items():
        metrics = evaluate_model(model, x_train, x_test, y_train, y_test)
        cv_scores = cross_val_score(model, x_raw, y, cv=cv, scoring="roc_auc", n_jobs=1)
        metrics["cv_roc_auc_mean"] = float(cv_scores.mean())
        metrics["cv_roc_auc_std"] = float(cv_scores.std())
        results[name] = metrics
        fitted[name] = model

    selected_name = "random_forest"
    selected = fitted[selected_name]
    scaler = selected.named_steps["standardscaler"]
    forest = selected.named_steps["randomforestclassifier"]

    bounds = domain_bounds(x_raw, FEATURES)
    all_probabilities = selected.predict_proba(x_raw)[:, 1]
    all_labels = selected.predict(x_raw)

    header, roots, sizes, nodes = generate_header(
        forest, scaler, bounds, "rf-v2-2026", 0, FEATURES
    )
    checksum = zlib.crc32(header.encode("utf-8")) & 0xFFFFFFFF
    header, roots, sizes, nodes = generate_header(
        forest, scaler, bounds, "rf-v2-2026", checksum, FEATURES
    )

    header_path = Path(args.header)
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(header, encoding="utf-8")
    header_copy = Path(args.header_copy)
    header_copy.parent.mkdir(parents=True, exist_ok=True)
    header_copy.write_text(header, encoding="utf-8")

    vector_text = generate_test_vectors(x_raw, all_probabilities, all_labels)
    vector_path = Path(args.vectors)
    vector_path.parent.mkdir(parents=True, exist_ok=True)
    vector_path.write_text(vector_text, encoding="utf-8")

    metrics_path = Path(args.metrics)
    metrics_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "selected_model": selected_name,
        "features": FEATURES,
        "label_classes": list(label_encoder.classes_),
        "dataset_rows": int(len(data)),
        "tree_count": len(roots),
        "node_count": len(nodes),
        "tree_sizes": sizes,
        "model_version": "rf-v2-2026",
        "model_checksum": f"0x{checksum:08X}",
        "domain_bounds": [
            {"feature": name, "min": float(low), "max": float(high)}
            for name, (low, high) in zip(FEATURES, bounds)
        ],
        "metrics": results,
    }
    metrics_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    model_out = Path(args.model_out)
    model_out.parent.mkdir(parents=True, exist_ok=True)
    joblib.dump(
        {
            "scaler": scaler,
            "model": forest,
            "label_encoder": label_encoder,
            "features": FEATURES,
            "model_version": "rf-v2-2026",
            "checksum": checksum,
        },
        model_out,
    )

    print(f"selected model: {selected_name}")
    for name, metrics in results.items():
        print(
            f"{name}: accuracy={metrics['accuracy']:.4f} recall={metrics['recall']:.4f} "
            f"precision={metrics['precision']:.4f} auc={metrics['roc_auc']:.4f} "
            f"brier={metrics['brier']:.4f}"
        )
    print(f"trees={len(roots)} nodes={len(nodes)} checksum=0x{checksum:08X}")
    print(f"header: {header_path}")
    print(f"vectors: {vector_path}")
    print(f"metrics: {metrics_path}")
    print(f"model: {model_out}")


if __name__ == "__main__":
    main()
