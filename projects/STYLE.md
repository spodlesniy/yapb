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

## Modern C++20 conventions

Use standard C++20 features in independent projects where they simplify code while preserving clear ownership, validation and portable behavior.
This policy does not increase the standard of the legacy YaPB runtime.

- Prefer aggregate initialization and C++20 designated initializers for simple data structures when values are known at construction.
  Name fields when several adjacent members have the same type, and specify them in declaration order as required by C++20.
  With strict `-Wmissing-field-initializers` builds, explicitly initialize non-defaulted aggregate members rather than disabling compiler warnings.
- Use `std::span<const T>` for read-only, non-owning contiguous sequences when an interface need not depend on `std::vector`.
  Make sure the referenced storage outlives the span; use owning containers for stored data.
- Prefer `std::ranges` algorithms such as `count`, `all_of` and `any_of` over manual iterator pairs or loops when they express the same intent more clearly.
  Use `std::views` for short lazy pipelines when their lifetimes and iteration cost remain obvious.
- Prefer `const` for unmodified locals, narrow scopes and direct initialization to multi-step assignment when readable.
  Use `std::string_view` for non-owning text parameters where lifetime is controlled.
- Do not introduce constructors solely to initialize aggregate data or use templates, concepts, views or lambdas just to demonstrate language features.
  Retain explicit checks and casts at binary-format boundaries; modernization must not change artifact bytes, public contracts or numerical behavior.

## C++20 in tests

Tests may use the full supported C++20 toolbox more freely to reduce fixture construction and assertion boilerplate.
Prefer designated aggregate fixtures, `std::ranges` and `std::views`, generic lambdas, constrained `auto` / concepts, structured bindings and default comparisons when they make tests shorter and still explicit.
Keep expected values, failure messages and test setup understandable without tracing complex metaprogramming.
The same braces, blank-line rules, deterministic output, bounded lifetime and cross-compiler build requirements apply to test code.
No newer C++23 facilities are assumed by test targets.

## Tooling

Run a sufficiently new `clang-format` version that recognizes every option in the shared `.clang-format` before verifying final style.
If the installed formatter reports an unknown option, do not use its silent fallback to LLVM style as the canonical result.
Run `clang-tidy` with a build compilation database to check remaining brace-less control-flow statements.
The D206.2 formatting pass used clangd 17 with a compatible subset of the configuration for brace insertion, retained the original project-specific layout, and applied blank-line requirements separately.
