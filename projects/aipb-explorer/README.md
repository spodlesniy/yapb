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
