# AiPB Repository Instructions

This repository is the AiPB development fork of YaPB.

## Scope

- Work only in the fork `spodlesniy/yapb`.
- Never modify, push to, or otherwise make changes to the upstream `yapb/yapb` repository.
- These instructions are repository guidance for coding-agent work.
  Direct user, system, and developer instructions always take precedence.
- Read the relevant documentation under `docs/ai/` before making non-trivial changes.

## Development workflow

- **Exactly one new ordinary commit for each completed, named logical development step** (for example `D176.3`).
  Include every intended code, test, and documentation change in that single commit; prohibit intermediate, per-file, preparatory, checkpoint, and additional commits under the same step name.
- Keep steps small, coherent, and deeply checked; do not combine unrelated changes.
- Prepare and review the complete diff **before** publishing the step.
  For a multi-file change, create Git blobs and one tree from the verified HEAD, create exactly one commit with that HEAD as parent, and move the branch once using `force=false` and `expected_sha` set to that HEAD.
  Do not publish via individual GitHub Contents API `create_file`/`update_file` calls because each one produces its own commit.
- **Never rewrite published history**: prohibit force-push, force-update, force-with-lease, amend, rebase, reset, squash, and equivalent operations, even when fixing CI or earlier commit fragmentation.
- A post-publication correction must be a **new, separately named corrective step**, with its own exactly one ordinary commit; it is not a second commit for the original step.
- After publication, verify the exact branch HEAD and check automatic unit-test CI for that SHA as the normal validation gate.
  Pending, skipped, absent, or inaccessible checks do not count as a successful build.
- When CI fails, inspect the exact current CI logs and fix the root cause.
  Do not guess.
- Only the user manually starts the full multi-platform build (`build_target=full`).
  Agents must not dispatch or rerun it, including after changes to original YaPB production code.
  When a change may affect it, explain the reason and recommend that the user check the specific commit.
- Only the user manually starts the Windows x86 build (`build_target=windows-x86`, job `bot-windows-x86`) when a game-test build is needed, for example to collect a new JSONL capture.
  Agents must not dispatch or rerun it; they may recommend a game check and inspect a run the user started.
- Automatic test CI triggered by ordinary pushes remains the normal validation gate.
  It does not authorize an agent to start either manual build.
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
7. Publish exactly one commit for the completed named step using a single expected-HEAD-checked, non-forced ref update.
8. Verify the published SHA and actual automatic CI result; correct failures in a separately named step without rewriting history.
