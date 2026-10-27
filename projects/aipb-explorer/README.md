# AiPB Explorer

Standalone C++20 movement-exploration contracts and a bounded in-memory transition episode.

D211 provides `MovementState`, `MovementCommand`, `MovementResult`, `IExplorationEnvironment`, validation and `ExplorationEpisode`.
It does not implement GoldSrc physics, BSP map queries, an autonomous explorer or neural policy.
The C++ library is `aipb::explorer_core`.

## Build

From `projects/aipb-explorer/`:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Use `release` instead of `debug` for the optimized build.
See [Exploration specification](../../docs/ai/INDEPENDENT_AI_EXPLORATION.md).

## D212 — Engine movement experiments

`aipb::explorer_goldsrc` provides a standalone callback adapter tested without the game engine.
The YaPB DLL provides a separate admin-only `yb explore` command for bounded authoritative GoldSrc movement tests.
It does not yet wire the C++20 callbacks into the game or support persistent replays.
See [D212 movement harness](../../docs/ai/INDEPENDENT_AI_EXPLORATION.md).

## D213 — Trace evidence and replay

The C++17-compatible `engine_bridge_trace.h` owns a bounded in-memory trace contract and tolerance comparisons, tested by `aipb_explorer_bridge_trace`.
The YaPB game probe records actual per-step input and engine states, supports `yb explore save` to export CSV and `yb explore replay <bot_index>` to re-execute the in-memory trace after returning to the original start.
Replay is diagnostic and fails on position/velocity/angle/grounded/duck divergence; there is no teleporting or guaranteed deterministic GoldSrc state restoration.
