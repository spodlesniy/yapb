#!/usr/bin/env python3
"""ONNX export and validation for the AiPB policy model."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .model_contract import (
    MODEL_INPUT_DTYPE,
    MODEL_INPUT_NAME,
    MODEL_ONNX_OPSET_VERSION,
    MODEL_OUTPUT_DTYPE,
    MODEL_OUTPUT_NAME,
    MODEL_RUNTIME_INPUT_SHAPE,
    MODEL_RUNTIME_OUTPUT_SHAPE,
)
from .policy_model import build_policy_model
from .trainer import create_optimizer
from .training_run import load_checkpoint


@dataclass(frozen=True)
class OnnxModelMetadata:
    input_name: str
    input_dtype: str
    input_shape: tuple[int, ...]
    output_name: str
    output_dtype: str
    output_shape: tuple[int, ...]


def _require_onnx():
    try:
        import onnx
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "ONNX validation requires the onnx package. "
            "Install tools/aipb_training/requirements.txt."
        ) from exc
    return onnx


def _require_export_dependencies():
    try:
        import onnxruntime
        import torch
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "ONNX export requires PyTorch and ONNX Runtime. "
            "Install tools/aipb_training/requirements.txt."
        ) from exc
    return torch, onnxruntime


def _value_info_metadata(value_info, onnx):
    tensor = value_info.type.tensor_type
    if tensor.elem_type != onnx.TensorProto.FLOAT:
        dtype = f"onnx_type_{tensor.elem_type}"
    else:
        dtype = "float32"

    shape: list[int] = []
    for dimension in tensor.shape.dim:
        if not dimension.HasField("dim_value"):
            raise ValueError(f"ONNX tensor {value_info.name} must use a static shape")
        shape.append(int(dimension.dim_value))

    return value_info.name, dtype, tuple(shape)


def validate_onnx_model(path: str | Path) -> OnnxModelMetadata:
    """Validate that an ONNX file matches the deployed AiPB runtime contract."""
    onnx = _require_onnx()
    model = onnx.load(Path(path))
    onnx.checker.check_model(model)

    if len(model.graph.input) != 1 or len(model.graph.output) != 1:
        raise ValueError("ONNX model must expose exactly one input and one output")

    input_name, input_dtype, input_shape = _value_info_metadata(model.graph.input[0], onnx)
    output_name, output_dtype, output_shape = _value_info_metadata(model.graph.output[0], onnx)

    metadata = OnnxModelMetadata(
        input_name=input_name,
        input_dtype=input_dtype,
        input_shape=input_shape,
        output_name=output_name,
        output_dtype=output_dtype,
        output_shape=output_shape,
    )

    if metadata.input_name != MODEL_INPUT_NAME:
        raise ValueError(f"ONNX input name must be {MODEL_INPUT_NAME!r}")
    if metadata.input_dtype != MODEL_INPUT_DTYPE:
        raise ValueError("ONNX input must use float32")
    if metadata.input_shape != MODEL_RUNTIME_INPUT_SHAPE:
        raise ValueError(
            f"ONNX input shape must be {MODEL_RUNTIME_INPUT_SHAPE}, got {metadata.input_shape}"
        )
    if metadata.output_name != MODEL_OUTPUT_NAME:
        raise ValueError(f"ONNX output name must be {MODEL_OUTPUT_NAME!r}")
    if metadata.output_dtype != MODEL_OUTPUT_DTYPE:
        raise ValueError("ONNX output must use float32")
    if metadata.output_shape != MODEL_RUNTIME_OUTPUT_SHAPE:
        raise ValueError(
            f"ONNX output shape must be {MODEL_RUNTIME_OUTPUT_SHAPE}, got {metadata.output_shape}"
        )

    return metadata


def export_checkpoint_to_onnx(
    checkpoint_path: str | Path,
    output_path: str | Path,
    device: str = "cpu",
    verify: bool = True,
) -> OnnxModelMetadata:
    """Export a compatible policy checkpoint to a static AiPB ONNX model."""
    torch, onnxruntime = _require_export_dependencies()
    model = build_policy_model().to(device)
    optimizer = create_optimizer(model)
    load_checkpoint(checkpoint_path, model, optimizer, device)
    model.eval()

    example_input = torch.zeros(
        MODEL_RUNTIME_INPUT_SHAPE,
        dtype=torch.float32,
        device=device,
    )

    try:
        onnx_program = torch.onnx.export(
            model,
            (example_input,),
            f=None,
            input_names=[MODEL_INPUT_NAME],
            output_names=[MODEL_OUTPUT_NAME],
            opset_version=MODEL_ONNX_OPSET_VERSION,
            dynamo=True,
        )
    except Exception as exc:
        raise RuntimeError(f"failed to export checkpoint as ONNX: {exc}") from exc

    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    onnx_program.save(output)
    metadata = validate_onnx_model(output)

    if verify:
        runtime_session = onnxruntime.InferenceSession(
            str(output), providers=["CPUExecutionProvider"]
        )
        runtime_output = runtime_session.run(
            [MODEL_OUTPUT_NAME],
            {MODEL_INPUT_NAME: example_input.detach().cpu().numpy()},
        )[0]
        torch_output = model(example_input).detach().cpu()
        runtime_tensor = torch.from_numpy(runtime_output)
        torch.testing.assert_close(runtime_tensor, torch_output, rtol=1e-4, atol=1e-5)

    return metadata


def validate_checkpoint_and_export(
    checkpoint_path: str | Path,
    output_path: str | Path,
    device: str = "cpu",
) -> OnnxModelMetadata:
    """Export a checkpoint and verify the complete ONNX deployment contract."""
    return export_checkpoint_to_onnx(
        checkpoint_path, output_path, device=device, verify=True
    )
