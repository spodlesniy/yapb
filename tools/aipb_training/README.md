# AiPB Python training package

This directory contains the offline training side of AiPB.
It does not participate in the Counter-Strike 1.6 game process.

## Responsibilities

The C++ AiPB runtime collects training transitions and exports them as aipb-training-jsonl.
The Python package validates, loads, batches, and eventually trains the policy model.
A trained model is exported to ONNX and loaded back by the C++ runtime.

The package boundary is:

C++ runtime -> JSONL -> Python training package -> ONNX -> C++ runtime

## Package structure

    tools/aipb_training/
    ├── __init__.py
    ├── README.md
    ├── requirements.txt
    ├── validate_dataset.py       # JSONL contract validation
    ├── dataset.py                # typed samples and deterministic batching
    ├── model_contract.py         # stable model I/O contract
    ├── feature_contract.py       # ordered semantic feature names
    ├── policy_model.py           # initial policy network
    ├── training_contract.py      # dataset batch -> model target encoding
    ├── trainer.py                # loss, optimizer, and low-level training loop
    ├── training_run.py           # splitting, epochs, checkpoints, and resume
    ├── train.py                  # training command-line entry point
    ├── onnx_export.py            # ONNX export and deployment validation
    ├── export.py                 # ONNX export command-line entry point
    ├── deploy.py                 # validated ONNX deployment into the package tree
    ├── evaluation.py             # checkpoint evaluation metrics
    ├── dataset_stats.py          # dataset composition and action coverage
    ├── dataset_quality.py        # configurable dataset quality thresholds
    └── tests/
        ├── __init__.py
        ├── test_validate_dataset.py
        ├── test_dataset.py
        ├── test_model_contract.py
        ├── test_policy_model.py
        ├── test_training_contract.py
        ├── test_training_run.py
        ├── test_onnx_export.py
        ├── test_export.py
        ├── test_pipeline.py
        └── test_evaluation.py

These components are intentionally separated so the model I/O contract remains independent from the training loop.

## Training core

The initial training core uses a robust SmoothL1 loss over the ten-value raw action tensor and AdamW with a learning rate of 1e-3 and weight decay of 1e-4.
One training pass updates model parameters; evaluation runs with gradients disabled and restores the model's previous training/evaluation state.

The current trainer operates on already batched `PolicyTrainingBatch` values.
Dataset splitting, checkpointing, and ONNX export are separate concerns and are not performed by this low-level training core.

## Training data

The current dataset record contains:

- observation
- action
- reward
- next_observation
- result
- elapsed_time
- terminal

The current policy training contract uses only observation and action as supervised input/target data.
Every exported sample is terminal because the runtime recorder stores only terminal action results; the validator enforces this invariant.
Reward and transition fields remain in the dataset because they are part of the runtime training record and can support later training methods.

## Policy model

The first policy model is a small feed-forward network intended as a baseline for supervised behavior cloning:

    input [N, 243]
        -> LayerNorm(243)
        -> Linear(243, 256) + ReLU
        -> Linear(256, 256) + ReLU
        -> Linear(256, 128) + ReLU
        -> Linear(128, 10)
        -> output [N, 10]

The model has no dropout, recurrent state, or other inference-time state.
Its output remains the raw AiPB action tensor.

PyTorch is the training backend.
The pinned requirements use the CPU-only PyTorch wheel, so the training tools do not require CUDA or NVIDIA runtime libraries.
The deployment toolchain additionally uses ONNX, ONNX Script, and ONNX Runtime.
GPU-specific PyTorch builds can be installed separately when GPU training is desired.

## Policy model input

The exported policy model has exactly one input:

| Property | Contract |
| --- | --- |
| Name | input |
| Element type | float32 |
| Rank | 2 |
| Runtime shape | [1, 243] |
| Meaning | AiPB inference feature vector |

During Python training, a batch has shape [N, 243], where N is the training batch size.

## Policy model output

The exported policy model has exactly one output:

| Property | Contract |
| --- | --- |
| Name | output |
| Element type | float32 |
| Rank | 2 |
| Runtime shape | [1, 10] |
| Meaning | Raw AiPB action output |

The ten output positions are fixed:

| Index | Field |
| ---: | --- |
| 0 | action_id |
| 1 | target_node |
| 2 | target_player |
| 3 | target_position.x |
| 4 | target_position.y |
| 5 | target_position.z |
| 6 | weapon_type |
| 7 | grenade_type |
| 8 | duration |
| 9 | confidence |

Discrete fields are represented as float32 in the neural-network tensor because this is the existing C++ inference contract.
The C++ action decoder/validator remains responsible for interpreting and validating the raw output.

## Export command

Export a trained checkpoint to a deployment model:

    python -m tools.aipb_training.export checkpoints/best.pt policy.onnx

The command validates the ONNX graph and runtime contract and verifies output parity with ONNX Runtime before returning successfully.

## Model feature schema

The current model input is schema version 5 with 243 ordered features.
Each observed player slot includes an `is_follow_target` flag so FollowPlayer actions can identify their target without relying on unstable player-slot ordering.

## In-game collection status

During a Training session, inspect the bounded in-memory collector from the server console:

    ai_training_status

The command reports buffered transitions, unique buffered episode IDs, total buffer capacity, and transitions dropped after the buffer became full.
It does not save or clear data.

## Dataset statistics

Inspect dataset composition before training:

    python -m tools.aipb_training.dataset_stats dataset.jsonl

The report shows sample and episode counts, terminal transition count, and how many of the 26 supported action IDs are actually represented, including per-action counts and percentages.
`Hide` is the appended action ID 25; existing IDs 0–24 remain stable.

## Dataset quality gate

Use explicit thresholds for an experiment without changing schema validation:

    python -m tools.aipb_training.dataset_quality dataset.jsonl --min-samples 10000 --min-episodes 100 --max-dominant-action-share 0.8

The gate does not require all 26 actions because the current deterministic teacher does not yet produce every model action.
Selected actions can be required explicitly with repeated `--min-action-samples ID:COUNT` and `--min-action-episodes ID:COUNT` options.
Episode thresholds are useful because train/validation splitting is episode-level.

The training command accepts the same thresholds and refuses to start when they fail:

    python -m tools.aipb_training.train dataset.jsonl --min-samples 10000 --min-episodes 100 --max-dominant-action-share 0.8 --min-action-samples 1:500 --min-action-samples 8:250 --min-action-episodes 1:20 --min-action-episodes 8:10

## Evaluation

Evaluate a trained checkpoint on the same deterministic episode split used by its training configuration:

    python -m tools.aipb_training.evaluation dataset.jsonl checkpoints/best.pt

The default split is validation.
The report includes SmoothL1 loss, overall mean absolute error, action ID accuracy using the runtime's float-to-integer truncation behavior, and mean absolute error for each of the ten model outputs.
The `--split train` option is available for comparing training-set and held-out behavior.

## Deployment

Deploy an already exported model into the standard package tree:

    python -m tools.aipb_training.deploy policy.onnx

The deployment command validates the ONNX runtime contract before copying the model to `cfg/addons/yapb/data/models/aipb_policy.onnx`.
The runtime uses this same path as the default `ai_model` value.
Because the release packager copies the repository `cfg` tree into the package, the deployed model is included in normal YaPB packages.

Use `--output` to place the model at another path when preparing a custom package.

## Export requirement

The ONNX exporter uses the PyTorch dynamo exporter with explicit opset 18, saves a static model, checks the ONNX graph, and verifies output parity with ONNX Runtime.

Training may use batches with arbitrary N, but the deployed ONNX model must satisfy the runtime contract exactly: one input [1,243] float32 and one output [1,10] float32.

A model with a dynamic or non-singleton runtime batch dimension is not compatible with the current C++ ONNX runner.
