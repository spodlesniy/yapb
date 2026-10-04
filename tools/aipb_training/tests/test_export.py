#!/usr/bin/env python3
"""Unit tests for the AiPB ONNX export CLI."""

from __future__ import annotations

import unittest
from unittest.mock import patch

from ..export import build_parser, main
from ..onnx_export import OnnxModelMetadata


class ExportCliTests(unittest.TestCase):
    def test_parser_requires_checkpoint_and_output(self) -> None:
        args = build_parser().parse_args(["checkpoint.pt", "policy.onnx"])

        self.assertEqual(args.checkpoint, "checkpoint.pt")
        self.assertEqual(args.output, "policy.onnx")
        self.assertEqual(args.device, "cpu")

    @patch("tools.aipb_training.export.export_checkpoint_to_onnx")
    @patch("tools.aipb_training.export.build_parser")
    def test_main_exports_requested_checkpoint(
        self,
        build_parser_mock,
        export_checkpoint_mock,
    ) -> None:
        build_parser_mock.return_value.parse_args.return_value = type(
            "Args",
            (),
            {
                "checkpoint": "checkpoint.pt",
                "output": "policy.onnx",
                "device": "cpu",
            },
        )()
        export_checkpoint_mock.return_value = OnnxModelMetadata(
            input_name="input",
            input_dtype="float32",
            input_shape=(1, 232),
            output_name="output",
            output_dtype="float32",
            output_shape=(1, 10),
        )

        self.assertEqual(main(), 0)
        export_checkpoint_mock.assert_called_once_with(
            "checkpoint.pt",
            "policy.onnx",
            device="cpu",
        )


if __name__ == "__main__":
    unittest.main()
