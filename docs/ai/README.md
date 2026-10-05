# AiPB AI Development Documentation

This directory contains durable project guidance for AI-assisted development of the AiPB fork.

## Documents

- [Architecture](ARCHITECTURE.md) — current AI runtime, policy, action, training responsibilities, offline Python training, and model I/O.
- [Development](DEVELOPMENT.md) — iteration, commit, CI, review, and verification workflow.
- [Coding Rules](CODING_RULES.md) — repository-specific implementation constraints and source-code conventions.
- [Decisions](DECISIONS.md) — important decisions and their rationale so future work does not accidentally revert them.

The offline Python training package itself is documented in [tools/aipb_training/README.md](../../tools/aipb_training/README.md).

## Source of truth

`AGENTS.md` is the concise entry point for coding-agent instructions.
The documents in this directory hold the detailed, durable project context.

Keep agent-facing instructions concise and actionable.
Put larger explanations and historical context here rather than turning `AGENTS.md` into a large project manual.
