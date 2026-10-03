#!/usr/bin/env python3
"""End-to-end smoke test for the AiPB offline training and deployment pipeline."""

from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

from ..dataset import load_training_dataset
from ..deploy import deploy_model
from ..model_contract import MODEL_FEATURE_COUNT
from ..onnx_export import validate_onnx_model
from ..training_run import TrainingConfig, run_training

ONNX_STACK_AVAILABLE = all(
    importlib.util.find_spec(name) is not None
    for name in ("torch", "onnx", "onnxscript", "onnxruntime")
)


def _make_sample(episode_id: int, index: int) -> dict:
    observation = [float(episode_id), float(index)] + [0.0] * (MODEL_FEATURE_COUNT - 2)
    next_observation = [float(episode_id), float(index + 1)] + [0.0] * (MODEL_FEATURE_COUNT - 2)

    return {
        "episode_id": episode_id,
        "observation": {
            "schema_version": 1,
            "values": observation,
        },
        "action": {
            "schema_version": 1,
            "action_id": 1,
            "target_node": 42,
            "target_player": -1,
            "target_position": [10.0, 20.0, 30.0],
            "weapon_type": 1,
            "grenade_type": 0,
            "duration": 0.8,
            "confidence": 0.8,
        },
        "reward": 1.0,
        "next_observation": {
            "schema_version": 1,
            "values": next_observation,
        },
        "result": 2,
        "elapsed_time": 0.1,
        "terminal": index == 1,
    }


def _write_dataset(path: Path) -> None:
    metadata = {
        "format": "aipb-training-jsonl",
        "version": 1,
        "feature_schema_version": 1,
        "action_schema_version": 1,
        "type": "metadata",
    }

    with path.open("w", encoding="utf-8") as stream:
        stream.write(json.dumps(metadata) + "\n")
        for episode_id in range(1, 5):
            for index in range(2):
                stream.write(json.dumps(_make_sample(episode_id, index)) + "\n")


@unittest.skipUnless(ONNX_STACK_AVAILABLE, "full training/export stack is not installed")
class OfflinePipelineSmokeTests(unittest.TestCase):
    def test_dataset_to_trained_onnx_to_deployed_model(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dataset_path = root / "dataset.jsonl"
            checkpoint_dir = root / "checkpoints"
            onnx_path = root / "policy.onnx"
            deployed_path = root / "package" / "cfg" / "addons" / "yapb" / "data" / "models" / "aipb_policy.onnx"

            _write_dataset(dataset_path)

            metadata, samples = load_training_dataset(dataset_path)
            self.assertEqual(metadata.version, 1)
            self.assertEqual(len(samples), 8)

            result = run_training(
                samples,
                TrainingConfig(
                    epochs=1,
                    batch_size=2,
                    validation_split=0.25,
                    seed=1234,
                ),
                checkpoint_dir,
            )

            self.assertIsNotNone(result.best_checkpoint_path)
            self.assertTrue(result.best_checkpoint_path.is_file())

            model_metadata = __import__(
                "tools.aipb_training.onnx_export",
                fromlist=["export_checkpoint_to_onnx"],
            ).export_checkpoint_to_onnx(
                result.best_checkpoint_path,
                onnx_path,
            )
            self.assertEqual(model_metadata.input_shape, (1, MODEL_FEATURE_COUNT))
            self.assertEqual(model_metadata.output_shape, (1, 10))

            validated = validate_onnx_model(onnx_path)
            self.assertEqual(validated.input_name, "input")
            self.assertEqual(validated.output_name, "output")

            deployed = deploy_model(onnx_path, deployed_path)
            self.assertEqual(deployed, validated)
            self.assertTrue(deployed_path.is_file())
            self.assertEqual(deployed_path.read_bytes(), onnx_path.read_bytes())


if __name__ == "__main__":
    unittest.main()
