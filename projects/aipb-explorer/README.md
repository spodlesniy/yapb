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
