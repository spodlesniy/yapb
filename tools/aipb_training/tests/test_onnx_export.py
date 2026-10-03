#!/usr/bin/env python3
"""Unit tests for the AiPB ONNX exporter."""

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

from ..model_contract import (
    MODEL_INPUT_NAME,
    MODEL_ONNX_OPSET_VERSION,
    MODEL_OUTPUT_NAME,
    MODEL_RUNTIME_INPUT_SHAPE,
    MODEL_RUNTIME_OUTPUT_SHAPE,
)
from ..onnx_export import export_checkpoint_to_onnx, validate_onnx_model
from ..policy_model import build_policy_model
from ..trainer import TrainingMetrics, create_optimizer
from ..training_run import EpochMetrics, TrainingConfig, save_checkpoint

ONNX_STACK_AVAILABLE = all(
    importlib.util.find_spec(name) is not None
    for name in ("torch", "onnx", "onnxscript", "onnxruntime")
)


@unittest.skipUnless(ONNX_STACK_AVAILABLE, "ONNX export dependencies are not installed")
class OnnxExporterTests(unittest.TestCase):
    def make_checkpoint(self, directory: Path) -> Path:
        import torch

        torch.manual_seed(1234)
        model = build_policy_model()
        optimizer = create_optimizer(model)
        metrics = EpochMetrics(
            epoch=1,
            train=TrainingMetrics(loss=0.5, samples=1),
            validation=TrainingMetrics(loss=0.6, samples=1),
        )
        return save_checkpoint(
            directory / "checkpoint.pt",
            model,
            optimizer,
            1,
            TrainingConfig(epochs=1),
            [metrics],
            1,
            0.6,
        )

    def test_export_matches_static_runtime_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checkpoint = self.make_checkpoint(root)
            output = root / "policy.onnx"

            metadata = export_checkpoint_to_onnx(checkpoint, output)

            self.assertEqual(metadata.input_name, MODEL_INPUT_NAME)
            self.assertEqual(metadata.output_name, MODEL_OUTPUT_NAME)
            self.assertEqual(metadata.input_shape, MODEL_RUNTIME_INPUT_SHAPE)
            self.assertEqual(metadata.output_shape, MODEL_RUNTIME_OUTPUT_SHAPE)
            self.assertTrue(output.is_file())

    def test_exported_model_passes_onnx_checker(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checkpoint = self.make_checkpoint(root)
            output = root / "policy.onnx"

            export_checkpoint_to_onnx(checkpoint, output)
            metadata = validate_onnx_model(output)

            self.assertEqual(metadata.input_dtype, "float32")
            self.assertEqual(metadata.output_dtype, "float32")

    def test_opset_is_explicit(self) -> None:
        self.assertEqual(MODEL_ONNX_OPSET_VERSION, 18)


if __name__ == "__main__":
    unittest.main()
