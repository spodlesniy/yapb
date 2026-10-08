# AiPB Development Workflow

## Iteration model

Each named development step (for example `D176.3`) is one coherent, reviewable logical change.
**One completed named step = exactly one new ordinary Git commit.**
That commit contains all intended code, focused tests, and documentation for the step.
No intermediate, per-file, checkpoint, preparatory, or additional commits are permitted within that same named step.
Do not publish any part of the step until the complete change has been prepared and reviewed.

One iteration should:

1. Define one narrow behavioral or structural change.
2. Inspect the relevant interfaces and dependencies before editing.
3. Inspect the affected `docs/ai` documentation and determine whether the current implementation changes require documentation updates.
4. Implement only that change.
5. Run focused validation locally when available.
6. Inspect the complete diff.
7. Publish exactly one commit for the completed named step via the atomic procedure below.
8. Verify that branch HEAD equals that commit, and check automatic unit-test CI for its exact SHA before starting the next step.
   Pending, skipped, missing, or inaccessible results are not passed CI.
   A correction discovered after publication becomes a new separately named corrective step, with its own single ordinary commit.

When an iteration changes a durable architecture, workflow, or engineering decision, update the corresponding `docs/ai` document in the same logical iteration.

For temporary or task-specific Git branches, delete the branch from the fork immediately after the task is fully completed and its changes are merged into the target branch.
This cleanup rule does not apply to permanent development branches.

Do not accumulate multiple unrelated fixes before committing.

## Atomic GitHub publication (mandatory)

The publication procedure for a completed named step is:

1. Read and verify the fork's target branch HEAD commit SHA and its tree SHA.
2. Prepare the full change, its tests, and documentation without moving the branch or creating intermediate commits.
3. Run available checks and review the complete diff; correct all identified pre-publication errors in the unpublished files.
4. Create Git blobs for the finalized files and one Git tree based on the verified HEAD tree, preserving untouched paths.
   Git blobs and trees are staging objects, not commits.
5. Create **exactly one commit** whose single parent is the verified HEAD and whose tree contains the complete step.
6. Update the branch ref **once** by non-forced fast-forward with `force=false` and `expected_sha=<verified HEAD>`.
7. Verify the new HEAD and published diff; check the automatic CI results for that exact commit SHA.

Do **not** use GitHub Contents API `create_file` or `update_file` for multi-file publication: each call creates its own commit.
Do not commit one file at a time merely because tool calls are limited.
If the expected HEAD changed, stop, inspect the new state, and rebuild the change without forcing a branch update.
**Never force-push, force-update, force-with-lease, amend, rebase, reset, squash, or otherwise rewrite published history.**
This prohibition also applies to consolidating mistakes and fixing failed CI.

## Verification failure handling

If validation fails **before publication**, diagnose the failure and correct the unpublished files without creating another commit.
If CI fails **after publication**, inspect the exact failing workflow and logs before changing code.
Use the actual compiler or test error to diagnose the problem, not assumptions.
Then define a **separately named corrective step**, prepare its complete fix and tests, and publish exactly one new ordinary commit for that new step.
Never append extra commits to the original step or rewrite its published commit.

## Pre-commit checks

For every touched source file, verify:

- The diff is limited to the current iteration.
- Standard-library includes appear before project/local includes.
- No `std::array` was introduced.
- Fixed-size arrays use ordinary C-style arrays.
- All source-code comments are written in English.
- Code that is explicitly known to be temporary, transitional, or scheduled for replacement must include a specific English `// TODO: ...` comment describing what will be replaced or implemented.
- New tests are syntactically balanced.
- Test functions are not duplicated.
- The relevant test target actually compiles and runs the changed tests.
- Python training orchestration tests must cover deterministic episode splitting and checkpoint behavior.
- The end-to-end training pipeline smoke test should exercise JSONL loading, one-epoch training, ONNX export/validation, and deployment into a temporary package tree.
- Evaluation tests must verify that checkpoint evaluation uses the checkpoint's deterministic episode split.

For Python training code, also verify:

- Production modules remain under `tools/aipb_training/`.
- Tests remain under `tools/aipb_training/tests/`.
- Training-core tests must remain runnable without PyTorch and skip only the framework-dependent execution tests when PyTorch is unavailable.
- The Python test directory is an importable package.
- CI discovers the intended `test_*.py` files.
- Package-qualified imports work from the repository root.

For tests, pay particular attention to fixture state, object lifecycle, braces, and accidental changes to neighboring test scopes.

## CI policy

The normal cycle is:

`complete named step -> exactly one commit -> automatic unit-test CI -> next named step`

The C++ AI unit-test build links the production `src/ai/ai_bot_action_executor.cpp` directly into the standalone AI test executable.
The executor depends only on the engine-independent `ActionExecutionContext`, so unit tests provide a mock context and exercise the production implementation without linking the game DLL.
The YaPB-specific adapter remains production-only.

A separate ONNX Runtime AI test job configures the same standalone test target with `-Donnxruntime=true` and executes the real `OnnxModelRunner` tests against the repository reference model.
The job uses the pinned ONNX Runtime 1.30.0 Linux x64 release archive, caches the archive and extracted runtime by exact version and SHA-256, and verifies the checksum before configuring a local pkg-config dependency on every run.
This keeps the normal unit-test job lightweight while ensuring the optional backend is continuously exercised in CI.

The Python training-tool job runs all tests under:

`tools/aipb_training/tests/`

using:

```text
python -m unittest discover -s tools/aipb_training/tests -t . -p 'test_*.py'
```

The training-tool job uses `actions/setup-python` pip caching with `tools/aipb_training/requirements.txt` as the dependency cache key input, so package downloads are reused until the requirements change.
The requirements use CPU-only PyTorch, avoiding CUDA/NVIDIA runtime downloads in the CI environment.
Test output is captured to `python-test.log` and uploaded as the `python-test-logs` artifact when the training-tool job fails.

When CI is red:

- Inspect the exact workflow run and failing log.
- The unit-test workflow runs Meson with `--print-errorlogs --verbose`.
- The unit-test job uploads `unit-tests/meson-logs/testlog.txt` as the `meson-test-logs` artifact on failure.
- Identify the actual compiler, linker, or test failure from the workflow log and, when needed, the uploaded Meson test log.
- Fix that root cause in a narrowly scoped corrective iteration.
- For automatic test CI, push the corrective commit and check the resulting automatic run before proceeding.
- For a user-started manual build, report the failure and recommend a user-run check of the corrective commit; do not dispatch or rerun the manual build.

Do not infer a failure from stale output or unrelated local assumptions.

## Full workflow

The full multi-platform workflow is intentionally used less frequently because it is more expensive.
Only the user manually starts this workflow with `build_target=full`.
Agents must not dispatch or rerun it, even when original YaPB production code was changed or a broader validation checkpoint is due.

An agent may recommend that the user check a specific commit when:

- a larger development block is complete;
- original YaPB production code was touched;
- a platform-specific regression needs validation;
- release or broad integration confidence is required.

State what could be affected and why the check is useful, then leave the launch decision to the user.
Inspect a user-started run when relevant and report its exact commit SHA and actual job conclusions.
An unrun or skipped full build is not a passing build and must not be described as validated.

Unit-test CI remains the normal feedback loop for small AI-layer iterations.
Automatic push-triggered tests do not authorize an agent to dispatch a manual build.

## Manual Windows x86 game-test build

Only the user manually starts `build_target=windows-x86` (job `bot-windows-x86`) when a build is needed for an in-game check, such as collecting a new training JSONL capture.
Agents must not dispatch or rerun this build.
An agent may explain why an in-game check would be useful, identify the commit to test, and inspect the run the user starts.
A code change, a missing JSONL capture, or a request to investigate gameplay does not authorize an agent to launch this build.
Keep successful compilation separate from actual gameplay validation and from obtaining the resulting capture.

## History-first platform compatibility

Before adding a compatibility macro, conditional compilation workaround, or platform-specific special case, search the repository history for prior attempts.

For Apple/libc++ compatibility, the established direction in this project was to remove unnecessary STL dependencies from the affected AI contracts and keep include ordering correct, rather than relying on a broad `CR_COMPAT_STL` workaround.

Do not reintroduce a workaround that history already showed to be inferior unless there is new evidence and the decision is explicitly revisited.

## GitHub scope

All work for this project must remain in:

`spodlesniy/yapb`

Never modify or push to:

`yapb/yapb`

The repository fork is the only intended development target for AiPB changes.

### Tool-operation budgets and substep planning

Tool and connector operation budgets are an execution constraint separate from Git history.
Plans must account for the available operation budget before starting a large edit.

For each iteration:

- Batch related repository reads before editing and reuse the returned contents instead of re-fetching unchanged files.
- Prepare the complete logical change before publishing it, so the publication phase does not consume operations on repeated discovery or corrective micro-edits.
- Prefer atomic Git Data publication for a multi-file logical change: build from the verified current parent tree, create the required objects, create one commit, and move the target branch once.
- Keep each implementation substep small enough to fit the currently available tool-operation budget, including the reads, edits, tests, documentation, and verification needed for that substep.
- When a task exceeds the available operation budget, plan **distinctly named and independently testable steps before publication**.
  Each step has its own coherent scope and exactly one commit.
  Never relabel file-by-file writes, partial implementations, or after-the-fact patches as substeps of an existing named step.
- Do not begin a publication sequence that is already likely to exceed the remaining operation budget.
  Reduce the number of files/operations through batching or move the next coherent substep to a new iteration.
- After publication, perform only the verification needed for that commit: confirm the branch head and inspect the resulting diff/status instead of repeating full discovery.

The exact operation budget can vary by tool/runtime and should not be assumed from a previous turn.
The development plan should therefore optimize for low operation count and bounded batches rather than depend on a fixed numeric limit.

## GitHub interaction and commit discipline

Current GitHub interaction limits are treated as an execution constraint, not as a reason to fragment the repository history.
Before editing, batch the required reads and avoid re-fetching unchanged files.
For a completed named development step, prepare implementation, tests, and documentation together and publish **exactly one commit**.
Never divide a step into intermediate or per-file commits to reduce API operations.
Use the atomic Git Data procedure above for all multi-file publications.
Corrections after publication are new separately named steps, each with one ordinary commit; published history cannot be rewritten.

When a larger validation checkpoint is required, use the resulting CI status to validate the single published commit rather than creating an extra checkpoint commit with no independent semantic change.

### Context and communication efficiency

Chat context is a limited engineering resource.
Development discussion should therefore stay concise and technical:

- report conclusions, decisions, failures, and required context rather than repeating already established project state;
- avoid restating documentation or previously agreed architecture unless it is needed for a new decision;
- batch repository reads and analyze files together before editing;
- prefer one complete technical update over several redundant intermediate messages.

This rule applies to planning and discussion only; source code, tests, and documentation must remain complete and sufficiently explicit.
