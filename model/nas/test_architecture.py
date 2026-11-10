import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from architecture import build_model, count_parameters, flatten_architecture, output_shape


class ArchitectureTests(unittest.TestCase):
    def test_forward_shape(self):
        model = build_model(12, [32, 16], "relu", 0.1)
        self.assertEqual(output_shape(model, 12), (2,))

    def test_parameter_count(self):
        model = build_model(12, [32, 16], "gelu", 0.0)
        expected = (12 * 32 + 32) + (32 * 16 + 16) + (16 + 1)
        self.assertEqual(count_parameters(model), expected)

    def test_flatten_architecture(self):
        self.assertEqual(flatten_architecture([64, 32, 16]), "64-32-16")

    def test_invalid_activation(self):
        with self.assertRaises(ValueError):
            build_model(12, [32], "sigmoid", 0.0)

    def test_invalid_dropout(self):
        with self.assertRaises(ValueError):
            build_model(12, [32], "relu", 1.0)


if __name__ == "__main__":
    unittest.main()
