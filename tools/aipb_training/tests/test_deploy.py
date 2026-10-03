#!/usr/bin/env python3
"""Unit tests for the AiPB ONNX deployment CLI."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from ..deploy import DEFAULT_MODEL_DESTINATION, build_parser, deploy_model, main
from ..onnx_export import OnnxModelMetadata


class DeployTests(unittest.TestCase):
    def test_default_destination(self) -> None:
        args = build_parser().parse_args(["policy.onnx"])

        self.assertEqual(Path(args.output), DEFAULT_MODEL_DESTINATION)

    @patch("tools.aipb_training.deploy.validate_onnx_model")
    def test_deploy_validates_before_copying(self, validate_model) -> None:
        metadata = OnnxModelMetadata(
            input_name="input",
            input_dtype="float32",
            input_shape=(1, 230),
            output_name="output",
            output_dtype="float32",
            output_shape=(1, 10),
        )
        validate_model.return_value = metadata

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "policy.onnx"
            destination = root / "models" / "aipb_policy.onnx"
            source.write_bytes(b"onnx")

            result = deploy_model(source, destination)

            self.assertEqual(result, metadata)
            validate_model.assert_called_once_with(source)
            self.assertEqual(destination.read_bytes(), b"onnx")

    @patch("tools.aipb_training.deploy.deploy_model")
    @patch("tools.aipb_training.deploy.build_parser")
    def test_main_deploys_requested_model(self, build_parser_mock, deploy_model_mock) -> None:
        build_parser_mock.return_value.parse_args.return_value = type(
            "Args",
            (),
            {
                "source": "policy.onnx",
                "output": str(DEFAULT_MODEL_DESTINATION),
            },
        )()
        deploy_model_mock.return_value = OnnxModelMetadata(
            input_name="input",
            input_dtype="float32",
            input_shape=(1, 230),
            output_name="output",
            output_dtype="float32",
            output_shape=(1, 10),
        )

        self.assertEqual(main(), 0)
        deploy_model_mock.assert_called_once_with(
            "policy.onnx",
            str(DEFAULT_MODEL_DESTINATION),
        )


if __name__ == "__main__":
    unittest.main()
