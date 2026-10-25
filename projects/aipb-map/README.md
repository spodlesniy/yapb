# AiPB Map Tool

Independent, native C++20 offline map-analysis project for future map import, navigation generation, comparison, and export.
D203 adds independent `MapModel` and `NavigationModel` value types, structural validation, and deterministic JSON/JSONL artifact serialization with SHA-256 fingerprints.
No BSP importer, navigation builder, analyzer, or YaPB `.graph` exporter is implemented yet.
The current `aipb-map` executable supports `--help` and `--version` only.

## Build and test

Requirements: CMake 3.21+, Ninja, and a C++20 compiler (GCC, Clang, or MSVC).
No crlib, linkage, ONNX Runtime, YaPB SDK headers, or game installation is required.

From `projects/aipb-map/`:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/aipb-map --version
```

On Windows, run `build\\debug\\aipb-map.exe --version` in place of the last command.
The `release` preset provides a second independent configuration.
Build outputs are ignored under `build/`.

## Architecture boundaries

The `aipb_map_core` static library and CLI compile independently from the repository's root Meson and CMake configurations.
`aipb_map_core` now owns engine-independent geometry, map and navigation types, while the separate `aipb_map_io` library owns JSON/JSONL, schema/version checks, artifact digests and loading.
For details see [the D203 format contract](../../docs/ai/INDEPENDENT_AI_MAP_FORMAT.md).
Later modules will implement importers, builders, analyzers, and exporters.
External format constraints (such as YaPB node/link limits) belong only in the corresponding exporter.
Project-specific rules are in [AGENTS.md](AGENTS.md).
