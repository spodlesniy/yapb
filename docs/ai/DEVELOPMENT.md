# AiPB Development Workflow

## Iteration model

Each logical development iteration is intentionally small.

One iteration should:

1. Define one narrow behavioral or structural change.
2. Inspect the relevant interfaces and dependencies before editing.
3. Implement only that change.
4. Run focused validation locally when available.
5. Inspect the complete diff.
6. Commit exactly that logical iteration.
7. Push the commit and use the automatic unit-test CI result as the normal gate for the next iteration.

Do not accumulate multiple unrelated fixes before committing.

## Pre-commit checks

For every touched source file, verify:

- The diff is limited to the current iteration.
- Standard-library includes appear before project/local includes.
- No `std::array` was introduced.
- Fixed-size arrays use ordinary C-style arrays.
- All source-code comments are written in English.
- New tests are syntactically balanced.
- Test functions are not duplicated.
- The relevant test target actually compiles and runs the changed tests.

For tests, pay particular attention to fixture state, object lifecycle, braces, and accidental changes to neighboring test scopes.

## CI policy

The normal cycle is:

`small change -> one commit -> automatic unit-test CI -> next change`

When CI is red:

- Inspect the exact workflow run and failing log.
- Identify the actual compiler, linker, or test failure.
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
