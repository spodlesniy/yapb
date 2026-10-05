#!/usr/bin/env python3
"""Training primitives for the AiPB supervised policy baseline."""
from __future__ import annotations
from dataclasses import dataclass
from typing import Iterable
from .model_contract import MODEL_ACTION_ID_COUNT, MODEL_ACTION_TENSOR_SIZE, MODEL_FEATURE_COUNT
from .training_contract import PolicyTrainingBatch

@dataclass(frozen=True)
class TrainingMetrics:
    loss: float
    samples: int
    action_accuracy: float = 0.0

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

def _action_class_targets(targets):
    torch = _torch()
    action_ids = targets[:, 0]
    integer_ids = action_ids.to(torch.int64)
    if not torch.isfinite(action_ids).all() or not torch.equal(action_ids, integer_ids.to(torch.float32)):
        raise ValueError("action ID targets must be finite integers")
    if not ((integer_ids >= 0) & (integer_ids < MODEL_ACTION_ID_COUNT)).all():
        raise ValueError(f"action ID targets must be within [0, {MODEL_ACTION_ID_COUNT})")
    return integer_ids

def policy_loss(predictions, targets, action_logits):
    torch = _torch()
    if predictions.ndim != 2 or predictions.shape[1] != MODEL_ACTION_TENSOR_SIZE:
        raise ValueError(f"model output must have shape [N, {MODEL_ACTION_TENSOR_SIZE}], got {tuple(predictions.shape)}")
    if predictions.shape != targets.shape:
        raise ValueError(f"prediction and target shapes differ: {tuple(predictions.shape)} vs {tuple(targets.shape)}")
    if action_logits.ndim != 2 or action_logits.shape[1] != MODEL_ACTION_ID_COUNT:
        raise ValueError(f"action logits must have shape [N, {MODEL_ACTION_ID_COUNT}], got {tuple(action_logits.shape)}")
    if not torch.isfinite(predictions).all() or not torch.isfinite(targets).all():
        raise ValueError("training predictions and targets must be finite")
    action_ids = _action_class_targets(targets)
    classification_loss = torch.nn.functional.cross_entropy(action_logits, action_ids)
    parameter_loss = torch.nn.functional.smooth_l1_loss(predictions[:, 1:], targets[:, 1:])
    return classification_loss + parameter_loss

def train_epoch(model, batches: Iterable[PolicyTrainingBatch], optimizer, device: str = "cpu") -> TrainingMetrics:
    torch = _torch()
    model.train()
    total_loss = 0.0
    sample_count = 0
    action_correct = 0
    for batch in batches:
        if batch.size == 0:
            continue
        observations, targets = _batch_tensors(batch, device)
        optimizer.zero_grad(set_to_none=True)
        predictions, action_logits = model.forward_training(observations)
        loss = policy_loss(predictions, targets, action_logits)
        if not torch.isfinite(loss):
            raise ValueError("training loss is not finite")
        loss.backward()
        optimizer.step()
        count = batch.size
        total_loss += float(loss.detach().item()) * count
        sample_count += count
        action_correct += int((action_logits.argmax(dim=1) == _action_class_targets(targets)).sum().item())
    return TrainingMetrics(total_loss / sample_count if sample_count else 0.0, sample_count, action_correct / sample_count if sample_count else 0.0)

def evaluate(model, batches: Iterable[PolicyTrainingBatch], device: str = "cpu") -> TrainingMetrics:
    torch = _torch()
    was_training = model.training
    model.eval()
    total_loss = 0.0
    sample_count = 0
    action_correct = 0
    try:
        with torch.no_grad():
            for batch in batches:
                if batch.size == 0:
                    continue
                observations, targets = _batch_tensors(batch, device)
                predictions, action_logits = model.forward_training(observations)
                loss = policy_loss(predictions, targets, action_logits)
                if not torch.isfinite(loss):
                    raise ValueError("evaluation loss is not finite")
                count = batch.size
                total_loss += float(loss.item()) * count
                sample_count += count
                action_correct += int((action_logits.argmax(dim=1) == _action_class_targets(targets)).sum().item())
    finally:
        model.train(was_training)
    return TrainingMetrics(total_loss / sample_count if sample_count else 0.0, sample_count, action_correct / sample_count if sample_count else 0.0)

def create_optimizer(model, learning_rate: float = 1e-3, weight_decay: float = 1e-4):
    if learning_rate <= 0.0:
        raise ValueError("learning_rate must be positive")
    if weight_decay < 0.0:
        raise ValueError("weight_decay must be non-negative")
    torch = _torch()
    return torch.optim.AdamW(model.parameters(), lr=learning_rate, weight_decay=weight_decay)
