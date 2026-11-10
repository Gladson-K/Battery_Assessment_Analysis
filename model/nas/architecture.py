from __future__ import annotations

from typing import Iterable, Sequence

import torch
from torch import nn


ACTIVATIONS = {
    "relu": nn.ReLU,
    "gelu": nn.GELU,
    "tanh": nn.Tanh,
}


class BatteryMLP(nn.Module):
    def __init__(
        self,
        input_dim: int,
        hidden_layers: Sequence[int],
        activation: str = "relu",
        dropout: float = 0.0,
    ) -> None:
        super().__init__()
        if input_dim <= 0:
            raise ValueError("input_dim must be positive")
        if not hidden_layers or any(width <= 0 for width in hidden_layers):
            raise ValueError("hidden_layers must contain positive widths")
        if activation not in ACTIVATIONS:
            raise ValueError(f"unsupported activation: {activation}")
        if not 0.0 <= dropout < 1.0:
            raise ValueError("dropout must be in [0, 1)")

        layers: list[nn.Module] = []
        previous = input_dim
        for width in hidden_layers:
            layers.append(nn.Linear(previous, width))
            layers.append(ACTIVATIONS[activation]())
            if dropout > 0.0:
                layers.append(nn.Dropout(dropout))
            previous = width
        layers.append(nn.Linear(previous, 1))
        self.network = nn.Sequential(*layers)

    def forward(self, features: torch.Tensor) -> torch.Tensor:
        return self.network(features).squeeze(-1)


def build_model(
    input_dim: int,
    hidden_layers: Sequence[int],
    activation: str = "relu",
    dropout: float = 0.0,
) -> BatteryMLP:
    return BatteryMLP(input_dim, hidden_layers, activation, dropout)


def count_parameters(model: nn.Module) -> int:
    return sum(parameter.numel() for parameter in model.parameters() if parameter.requires_grad)


def output_shape(model: nn.Module, input_dim: int, batch_size: int = 2) -> tuple[int, ...]:
    model.eval()
    with torch.no_grad():
        result = model(torch.zeros(batch_size, input_dim, dtype=torch.float32))
    return tuple(result.shape)


def flatten_architecture(hidden_layers: Iterable[int]) -> str:
    return "-".join(str(int(width)) for width in hidden_layers)
