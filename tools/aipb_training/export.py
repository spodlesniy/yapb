#!/usr/bin/env python3
"""Command-line entry point for exporting an AiPB policy checkpoint to ONNX."""

from __future__ import annotations

if __name__ == "__main__" and not __package__:
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    __package__ = "tools.aipb_training"

import argparse

from .onnx_export import export_checkpoint_to_onnx


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Export a trained AiPB PyTorch checkpoint to a runtime-compatible ONNX model."
    )
    parser.add_argument("checkpoint", help="Path to a trained PyTorch checkpoint.")
    parser.add_argument("output", help="Path for the exported ONNX model.")
    parser.add_argument(
        "--device",
        default="cpu",
        help="PyTorch device used during export (default: cpu).",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    metadata = export_checkpoint_to_onnx(
        args.checkpoint,
        args.output,
        device=args.device,
    )

    print(f"onnx={args.output}")
    print(f"input={metadata.input_name} {metadata.input_dtype} {metadata.input_shape}")
    print(f"output={metadata.output_name} {metadata.output_dtype} {metadata.output_shape}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
