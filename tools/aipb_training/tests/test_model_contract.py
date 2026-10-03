#!/usr/bin/env python3
"""Unit tests for the AiPB model I/O contract."""

from __future__ import annotations

import unittest

from tools.aipb_training.model_contract import (
    MODEL_ACTION_TENSOR_SIZE,
    MODEL_FEATURE_COUNT,
    MODEL_INPUT_DTYPE,
    MODEL_INPUT_NAME,
    MODEL_OUTPUT_DTYPE,
    MODEL_OUTPUT_NAME,
    MODEL_RUNTIME_INPUT_SHAPE,
    MODEL_RUNTIME_OUTPUT_SHAPE,
    ModelOutputIndex,
    validate_feature_count,
)


class ModelContractTests(unittest.TestCase):
    def test_runtime_input_contract(self) -> None:
        self.assertEqual(MODEL_INPUT_NAME, "input")
        self.assertEqual(MODEL_INPUT_DTYPE, "float32")
        self.assertEqual(MODEL_RUNTIME_INPUT_SHAPE, (1, 230))
        self.assertEqual(MODEL_FEATURE_COUNT, 230)

    def test_runtime_output_contract(self) -> None:
        self.assertEqual(MODEL_OUTPUT_NAME, "output")
        self.assertEqual(MODEL_OUTPUT_DTYPE, "float32")
        self.assertEqual(MODEL_RUNTIME_OUTPUT_SHAPE, (1, 10))
        self.assertEqual(MODEL_ACTION_TENSOR_SIZE, 10)
        self.assertEqual(ModelOutputIndex.CONFIDENCE, 9)

    def test_feature_count_validation(self) -> None:
        validate_feature_count(230)
        with self.assertRaises(ValueError):
            validate_feature_count(229)


if __name__ == "__main__":
    unittest.main()
