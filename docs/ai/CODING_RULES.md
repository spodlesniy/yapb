# AiPB Coding Rules

## Mandatory rules

### Fixed-size arrays

Do not use:

`std::array`

Use an ordinary fixed-size C-style array when fixed storage is required, for example:

`float values[FEATURE_COUNT] {};`

Avoid introducing a standard-library container solely to represent fixed-capacity storage in low-level AI contracts.

### Include ordering

When a file needs both standard-library and project/local headers, order them as:

1. Standard-library headers.
2. Project and local headers.

For example:

`#include <cstddef>`
`#include <cstdint>`

followed by project headers such as:

`#include <ai/ai_action_result.h>`

Keep existing local/test-helper conventions intact unless the current change is specifically about include organization.

### Markdown source formatting

For Markdown documentation, write each prose sentence on its own physical line.
A normal source-level line break inside a paragraph must not be used as a Markdown hard break solely for formatting purposes.
Preserve blank lines between paragraphs and keep headings, list structure, tables, front matter, fenced and indented code blocks, link definitions, and other syntax-sensitive constructs intact.
When a documentation-only reformat is performed, verify that the existing non-whitespace content is unchanged before committing.

### Source-code comments

All comments in source code must be written in English.

Existing comments that are already in English should remain so.
New comments and comments modified during an iteration must also be English.

Comments should explain intent or non-obvious constraints rather than restating obvious code.

## Compatibility

Do not introduce compatibility flags or platform-specific workarounds without first checking repository history for an established solution.

When a platform issue can be resolved by removing an unnecessary dependency, prefer the dependency-removal approach over adding broad compatibility switches.

## API and architecture

- Keep public AI contracts explicit and minimal.
- Avoid unnecessary STL dependencies in low-level model-facing contracts.
- Keep runtime execution and training recording responsibilities separate.
- Preserve the legacy control path.
- Make lifecycle changes explicit and testable.

## Testing

When modifying tests:

- Keep each test focused on one behavior.
- Avoid duplicate test definitions.
- Verify fixture state before asserting lifecycle behavior.
- Keep scopes and braces balanced.
- Confirm the test target includes the file and the changed test actually executes.
