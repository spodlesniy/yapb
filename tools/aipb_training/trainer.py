#!/usr/bin/env python3
"""Training primitives for the AiPB supervised policy baseline."""
from __future__ import annotations
from dataclasses import dataclass
from typing import Iterable
from .model_contract import MODEL_ACTION_TENSOR_SIZE, MODEL_FEATURE_COUNT
from .training_contract import PolicyTrainingBatch

@dataclass(frozen=True)
class TrainingMetrics:
    loss: float
    samples: int

def _torch():
    try:
        import torch
    except ModuleNotFoundError as exc:
        raise RuntimeError("PyTorch is required for AiPB training. Install tools/aipb_training/requirements.txt.") from exc
    return torch

def _batch_tensors(batch: PolicyTrainingBatch, device: str):
    torch = _torch()
    observations = torch.tensor(batch.observations, dtype=torch.float32, device=device)
    targets = torch.tensor(batch.action_targets, dtype=torch.float32, device=device)
    if observations.ndim != 2 or observations.shape[1] != MODEL_FEATURE_COUNT:
        raise ValueError(f"training input must have shape [N, {MODEL_FEATURE_COUNT}], got {tuple(observations.shape)}")
    if targets.ndim != 2 or targets.shape[1] != MODEL_ACTION_TENSOR_SIZE:
        raise ValueError(f"training target must have shape [N, {MODEL_ACTION_TENSOR_SIZE}], got {tuple(targets.shape)}")
    return observations, targets

def policy_loss(predictions, targets):
    torch = _torch()
    if predictions.ndim != 2 or predictions.shape[1] != MODEL_ACTION_TENSOR_SIZE:
        raise ValueError(f"model output must have shape [N, {MODEL_ACTION_TENSOR_SIZE}], got {tuple(predictions.shape)}")
    if predictions.shape != targets.shape:
        raise ValueError(f"prediction and target shapes differ: {tuple(predictions.shape)} vs {tuple(targets.shape)}")
    if not torch.isfinite(predictions).all() or not torch.isfinite(targets).all():
        raise ValueError("training predictions and targets must be finite")
    return torch.nn.functional.smooth_l1_loss(predictions, targets)

def train_epoch(model, batches: Iterable[PolicyTrainingBatch], optimizer, device: str = "cpu") -> TrainingMetrics:
    torch = _torch()
    model.train()
    total_loss = 0.0
    sample_count = 0
    for batch in batches:
        if batch.size == 0:
            continue
        observations, targets = _batch_tensors(batch, device)
        optimizer.zero_grad(set_to_none=True)
        loss = policy_loss(model(observations), targets)
        if not torch.isfinite(loss):
            raise ValueError("training loss is not finite")
        loss.backward()
        optimizer.step()
        count = batch.size
        total_loss += float(loss.detach().item()) * count
        sample_count += count
    return TrainingMetrics(total_loss / sample_count if sample_count else 0.0, sample_count)

def evaluate(model, batches: Iterable[PolicyTrainingBatch], device: str = "cpu") -> TrainingMetrics:
    torch = _torch()
    was_training = model.training
    model.eval()
    total_loss = 0.0
    sample_count = 0
    try:
        with torch.no_grad():
            for batch in batches:
                if batch.size == 0:
                    continue
                observations, targets = _batch_tensors(batch, device)
                loss = policy_loss(model(observations), targets)
                if not torch.isfinite(loss):
                    raise ValueError("evaluation loss is not finite")
                count = batch.size
                total_loss += float(loss.item()) * count
                sample_count += count
    finally:
        model.train(was_training)
    return TrainingMetrics(total_loss / sample_count if sample_count else 0.0, sample_count)

def create_optimizer(model, learning_rate: float = 1e-3, weight_decay: float = 1e-4):
    if learning_rate <= 0.0:
        raise ValueError("learning_rate must be positive")
    if weight_decay < 0.0:
        raise ValueError("weight_decay must be non-negative")
    torch = _torch()
    return torch.optim.AdamW(model.parameters(), lr=learning_rate, weight_decay=weight_decay)
