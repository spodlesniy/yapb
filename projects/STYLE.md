# C++ style for standalone projects

This policy applies to C++ sources and tests under `projects/`, alongside the inherited `.clang-format` and `.clang-tidy` configurations.
It does not change the legacy YaPB source style under `src/` or `inc/`.

## Mandatory control-flow braces

Always use `{ ... }` for `if`, `else`, `for`, range-based `for`, `while`, and `do` statements, even if the body is a single statement.
Keep the `else` and `catch` connected to their preceding closing brace, and never insert an empty line between `}` and `else`/`catch`.
The project `.clang-format` sets `InsertBraces: true`; `.clang-tidy` enables `readability-braces-around-statements` and checks even short statements.
Do not use one-line control-flow bodies as an exception.

## Blank lines and logical sections

- Put one empty line before an `if`, `for`, `switch`, `while`, `do` or `try` block when it follows another complete statement or declaration.
- Do not force a blank line at the beginning of a function or block, after a directly related label or comment, or between the clauses of one `if` / `else if` / `else` chain.
- Put one empty line before a final `return` if other statements precede it in the same function.
  If `return` is the function's only statement, do not add a leading blank line.
- Separate distinct logical stages (for example, input validation, data preparation, iteration, and result construction) with a single empty line where it improves readability.
  Keep related statements together; do not introduce gratuitous vertical whitespace or multiple adjacent empty lines.
- Keep a related explanatory comment with its statement or block rather than inserting a blank line between them.

Whitespace between semantic sections cannot be determined reliably by clang-format alone.
Treat those line-spacing requirements as review rules, not as an automatically guaranteed clang-format transformation.

## Tooling

Run a sufficiently new `clang-format` version that recognizes every option in the shared `.clang-format` before verifying final style.
If the installed formatter reports an unknown option, do not use its silent fallback to LLVM style as the canonical result.
Run `clang-tidy` with a build compilation database to check remaining brace-less control-flow statements.
The D206.2 formatting pass used clangd 17 with a compatible subset of the configuration for brace insertion, retained the original project-specific layout, and applied blank-line requirements separately.
