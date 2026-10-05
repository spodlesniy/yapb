# AiPB Repository Instructions

This repository is the AiPB development fork of YaPB.

## Scope

- Work only in the fork `spodlesniy/yapb`.
- Never modify, push to, or otherwise make changes to the upstream `yapb/yapb` repository.
- These instructions are repository guidance for coding-agent work.
  Direct user, system, and developer instructions always take precedence.
- Read the relevant documentation under `docs/ai/` before making non-trivial changes.

## Development workflow

- One completed logical iteration must produce one commit.
- Keep iterations small, focused, and deeply checked.
- Do not combine unrelated changes into the same iteration or commit.
- After a commit is pushed, use the automatic unit-test CI result as the normal validation gate before starting the next iteration.
- When CI fails, inspect the exact current CI logs and fix the root cause.
  Do not guess.
- Run the full multi-platform workflow for the end of a larger development block, for changes that touch original YaPB production code, or when platform compatibility/regression coverage is specifically required.
- Before committing, inspect the complete diff and verify the intended test target, syntax, include ordering, fixed-size array usage, and scope of the change.

## Coding rules

- Do not use `std::array`.
- Use ordinary fixed-size C-style arrays when a fixed-size array is required.
- Put all standard-library includes before project and local includes, all else being equal.
- All comments written in source code must be in English.
- In Markdown files, write each prose sentence on its own physical line; preserve blank lines, headings, list structure, tables, front matter, code blocks, link definitions, and other syntax-sensitive Markdown constructs. Do not use Markdown hard breaks solely for source formatting.
- Preserve existing public APIs and architecture unless the current iteration explicitly requires an API or architectural change.
- Prefer engine-independent AI abstractions and keep engine-specific integration at the runtime boundary.
- Search the repository history for an established solution before introducing compatibility flags or platform-specific workarounds.

## AI architecture

The current AI architecture is organized around:

`Observation -> Policy -> Action -> validation/execution -> ActionResult`

Training adds transition recording around action execution:

`Observation -> Action -> execution -> reward -> TrainingTransition`

Current control modes are:

- `Legacy`: original YaPB behavior; AI control must not interfere with legacy behavior.
- `Neural`: AI behavior is produced by `InferencePolicy` when an inference provider is available.
- `Training`: the deterministic `GoalNavigationPolicy` currently supplies behavior while transitions are recorded for training.

The training lifecycle is based on:

`beginEpisode -> startAction -> execution -> terminal/cancel -> reward -> finishAction`

The training buffer is a fixed-capacity contiguous store exposed through read-only access.

## Repository-specific guidance

- Keep `Observation`, `Policy`, `Action`, validation/execution, `ActionResult`, training collection, reward calculation, and transition recording separated by responsibility.
- Do not put game-specific reward heuristics into the generic recorder.
  Reward calculation belongs behind the reward-provider abstraction.
- Waypoint data is part of the AI observation/navigation pipeline and should remain consumable without coupling the model-facing contracts to engine-specific implementation details.
- Preserve the distinction between runtime execution state and training-recording lifecycle state.
- When changing lifecycle behavior, inspect both `BotRuntime` and `TrainingCollector`/ `TrainingRecorder` so that state is not duplicated or prematurely finalized.

## Verification checklist

Before finalizing a change:

1. Confirm the diff contains only the intended iteration.
2. Confirm no `std::array` was introduced.
3. Confirm standard-library includes precede project/local includes in touched files.
4. Confirm all new or changed source-code comments are in English.
5. Confirm tests are syntactically balanced and do not contain duplicate test cases.
6. Confirm the relevant unit-test target includes the changed tests.
7. Push the commit and wait for the automatic unit-test CI result before beginning the next iteration.
8. For CI failures, use the exact failing log output to determine the fix.
