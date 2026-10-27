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

## D213.1 — Persistent named traces

Trace I/O contracts and CSV v2 parsing are in the C++17-compatible `engine_bridge_trace_io.h` with standalone regression tests.
Use `yb explore save <name>`, `yb explore list`, `yb explore init <name> <bot_index>` and `yb explore replay <name> <bot_index>` to work with named movement traces under YaPB data/explorer.
Init explicitly moves the bot, while replay rejects incompatible start states and reports divergence; the full game world is not reset.

## D213.3 — Clearing the current trace

`yb explore clear` discards the current in-memory recording only; named `.csv` files are not deleted.
`yb explore save <name>` never clears the buffer.
Each separate movement command still starts a fresh trace, not an appended multi-command recording.
Clear is refused while an experiment or replay is active.

## D213.5 — Heap-backed bounded trace storage

`TraceBuffer` owns a lazily allocated fixed-capacity array of up to 4096 steps rather than embedding it in the DLL's static data.
Its memory is released on `clear()` and ownership is moved after a successful named-trace load.
The C++17 bridge trace tests exercise descriptor size, allocation, ownership transfer, release and full-capacity bounds; CSV files and engine commands are unchanged.
