# AiPB Development Workflow

## Iteration model

Each logical development action is a coherent, reviewable unit. One primary commit may include the implementation, focused tests, and documentation needed to complete that action.

One iteration should:

1. Define one narrow behavioral or structural change.
2. Inspect the relevant interfaces and dependencies before editing.
3. Inspect the affected `docs/ai` documentation and determine whether the current implementation changes require documentation updates.
4. Implement only that change.
5. Run focused validation locally when available.
6. Inspect the complete diff.
7. Publish one primary commit for exactly that logical development action.
8. Push the commit and use the automatic unit-test CI result as the normal gate for the next action. If the published action later needs a correction, make it in a new ordinary corrective commit.

When an iteration changes a durable architecture, workflow, or engineering decision, update the corresponding `docs/ai` document in the same logical iteration.

For temporary or task-specific Git branches, delete the branch from the fork immediately after the task is fully completed and its changes are merged into the target branch. This cleanup rule does not apply to permanent development branches.

Do not accumulate multiple unrelated fixes before committing.

## Verification failure handling

When a test or CI job fails, inspect the actual failing test or workflow logs first. Treat the log output as the primary source for the failure diagnosis; do not speculate about the cause or modify code until the concrete failure is identified. After identifying the failure, make the smallest necessary correction in a new ordinary corrective commit and re-run the relevant validation. Do not rewrite or force-update the published commit to apply the correction.

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

`small change -> one commit -> automatic unit-test CI -> next change`

The C++ AI unit-test build links the production `src/ai/ai_bot_action_executor.cpp` directly into the standalone AI test executable. The executor depends only on the engine-independent `ActionExecutionContext`, so unit tests provide a mock context and exercise the production implementation without linking the game DLL. The YaPB-specific adapter remains production-only.

A separate ONNX Runtime AI test job configures the same standalone test target with `-Donnxruntime=true` and executes the real `OnnxModelRunner` tests against the repository reference model. The job uses the pinned ONNX Runtime 1.30.0 Linux x64 release archive and verifies its SHA-256 before configuring a local pkg-config dependency. This keeps the normal unit-test job lightweight while ensuring the optional backend is continuously exercised in CI.

The Python training-tool job runs all tests under:

`tools/aipb_training/tests/`

using:

```text
python -m unittest discover -s tools/aipb_training/tests -t . -p 'test_*.py'
```

The training-tool job uses `actions/setup-python` pip caching with `tools/aipb_training/requirements.txt` as the dependency cache key input, so package downloads are reused until the requirements change. The requirements use CPU-only PyTorch, avoiding CUDA/NVIDIA runtime downloads in the CI environment. Test output is captured to `python-test.log` and uploaded as the `python-test-logs` artifact when the training-tool job fails.

When CI is red:

- Inspect the exact workflow run and failing log.
- The unit-test workflow runs Meson with `--print-errorlogs --verbose`.
- The unit-test job uploads `unit-tests/meson-logs/testlog.txt` as the `meson-test-logs` artifact on failure.
- Identify the actual compiler, linker, or test failure from the workflow log and, when needed, the uploaded Meson test log.
- Fix that root cause in a narrowly scoped corrective iteration.
- Re-run CI before proceeding.

Do not infer a failure from stale output or unrelated local assumptions.

## Full workflow

The full multi-platform workflow is intentionally used less frequently because it is more expensive.

Use it when:

- a larger development block is complete;
- original YaPB production code was touched;
- a platform-specific regression needs validation;
- release or broad integration confidence is required.

Unit-test CI remains the normal feedback loop for small AI-layer iterations.

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

Tool and connector operation budgets are an execution constraint separate from Git history. Plans must account for the available operation budget before starting a large edit.

For each iteration:

- Batch related repository reads before editing and reuse the returned contents instead of re-fetching unchanged files.
- Prepare the complete logical change before publishing it, so the publication phase does not consume operations on repeated discovery or corrective micro-edits.
- Prefer atomic Git Data publication for a multi-file logical change: build from the verified current parent tree, create the required objects, create one commit, and move the target branch once.
- Keep each implementation substep small enough to fit the currently available tool-operation budget, including the reads, edits, tests, documentation, and verification needed for that substep.
- When a larger logical task would exceed the available operation budget, split it at a real semantic or testable boundary into multiple complete iterations. Each substep must leave the repository in a coherent state and may have its own single commit; do not split solely to create micro-commits or to repeat the same work.
- Do not begin a publication sequence that is already likely to exceed the remaining operation budget. Reduce the number of files/operations through batching or move the next coherent substep to a new iteration.
- After publication, perform only the verification needed for that commit: confirm the branch head and inspect the resulting diff/status instead of repeating full discovery.

The exact operation budget can vary by tool/runtime and should not be assumed from a previous turn. The development plan should therefore optimize for low operation count and bounded batches rather than depend on a fixed numeric limit.

## GitHub interaction and commit discipline

Current GitHub interaction limits are treated as an execution constraint, not as a reason to fragment the repository history. Before editing, batch the required reads and avoid re-fetching unchanged files. For a completed logical development action, prepare the full implementation, focused tests, and required documentation together, then publish one primary commit. Do not split one action into multiple micro-commits merely to reduce the size of individual API operations. Corrective changes discovered after publication are separate ordinary commits; do not rewrite or force-update the earlier commit for routine fixes.

When a larger validation checkpoint is required, use the resulting CI status to validate the single published commit rather than creating an extra checkpoint commit with no independent semantic change.

### Context and communication efficiency

Chat context is a limited engineering resource. Development discussion should therefore stay concise and technical:

- report conclusions, decisions, failures, and required context rather than repeating already established project state;
- avoid restating documentation or previously agreed architecture unless it is needed for a new decision;
- batch repository reads and analyze files together before editing;
- prefer one complete technical update over several redundant intermediate messages.

This rule applies to planning and discussion only; source code, tests, and documentation must remain complete and sufficiently explicit.
