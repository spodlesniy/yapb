#!/usr/bin/env python3
"""Deploy a validated AiPB ONNX model into the repository package tree."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

from .onnx_export import OnnxModelMetadata, validate_onnx_model

DEFAULT_MODEL_DESTINATION = Path("cfg/addons/yapb/data/models/aipb_policy.onnx")


def deploy_model(source: str | Path, destination: str | Path = DEFAULT_MODEL_DESTINATION) -> OnnxModelMetadata:
    source_path = Path(source)
    destination_path = Path(destination)

    metadata = validate_onnx_model(source_path)

    if source_path.resolve() != destination_path.resolve():
        destination_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_path, destination_path)

    return metadata


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate and deploy an AiPB ONNX model into the repository package tree."
    )
    parser.add_argument("source", help="Path to the exported ONNX model.")
    parser.add_argument(
        "--output",
        default=str(DEFAULT_MODEL_DESTINATION),
        help="Deployment path (default: cfg/addons/yapb/data/models/aipb_policy.onnx).",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    metadata = deploy_model(args.source, args.output)

    print(f"onnx={args.output}")
    print(f"input={metadata.input_name} {metadata.input_dtype} {metadata.input_shape}")
    print(f"output={metadata.output_name} {metadata.output_dtype} {metadata.output_shape}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
