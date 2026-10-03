# AiPB Python training package

This directory contains the offline training side of AiPB. It does not participate in the Counter-Strike 1.6 game process.

## Responsibilities

The C++ AiPB runtime collects training transitions and exports them as aipb-training-jsonl. The Python package validates, loads, batches, and eventually trains the policy model. A trained model is exported to ONNX and loaded back by the C++ runtime.

The package boundary is:

C++ runtime -> JSONL -> Python training package -> ONNX -> C++ runtime

## Package structure

    tools/aipb_training/
    ├── __init__.py
    ├── validate_dataset.py       # JSONL contract validation
    ├── dataset.py                # typed samples and deterministic batching
    ├── model_contract.py         # stable model I/O contract
    ├── training_contract.py      # dataset batch -> model target encoding
    └── tests/
        ├── test_validate_dataset.py
        ├── test_dataset.py
        ├── test_model_contract.py
        └── test_training_contract.py

Future training components belong here as separate modules:

- trainer.py: model construction, loss, optimizer, training loop
- evaluate.py: offline evaluation
- export.py: model export and ONNX contract checks

These components are intentionally not added until the model contract is fixed.

## Training data

The current dataset record contains:

- observation
- action
- reward
- next_observation
- result
- elapsed_time
- terminal

The current policy training contract uses only observation and action as supervised input/target data. Reward and transition fields remain in the dataset because they are part of the runtime training record and can support later training methods.

## Policy model input

The exported policy model has exactly one input:

| Property | Contract |
| --- | --- |
| Name | input |
| Element type | float32 |
| Rank | 2 |
| Runtime shape | [1, 230] |
| Meaning | AiPB inference feature vector |

During Python training, a batch has shape [N, 230], where N is the training batch size.

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

Discrete fields are represented as float32 in the neural-network tensor because this is the existing C++ inference contract. The C++ action decoder/validator remains responsible for interpreting and validating the raw output.

## Export requirement

Training may use batches with arbitrary N, but the deployed ONNX model must satisfy the runtime contract exactly: one input [1,230] float32 and one output [1,10] float32.

A model with a dynamic or non-singleton runtime batch dimension is not compatible with the current C++ ONNX runner.
