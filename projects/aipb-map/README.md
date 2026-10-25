# AiPB Map Tool

Independent native C++20 tool for offline map import, navigation generation, comparison and export.
D203 provides engine-independent `MapModel` and `NavigationModel`, validation and deterministic `.aipbmap`/`.aipbnav` JSONL artifacts with SHA-256 fingerprints.
D204 adds a separate GoldSrc BSP v30 importer and the `aipb-map import` CLI command.
Navigation builders, collision hull import, physics validation and YaPB `.graph` export remain future work.

## Build and test

Requirements: CMake 3.21+, Ninja and a C++20 compiler (GCC, Clang or MSVC).
No crlib, linkage, ONNX Runtime, YaPB/GoldSrc SDK headers or game installation is needed.

From `projects/aipb-map/`:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/aipb-map --help
```

On Windows, use `build\debug\aipb-map.exe` in place of `./build/debug/aipb-map`.
The `release` preset is also available; generated output is ignored under `build/`.

## Import GoldSrc BSP v30

```sh
./build/debug/aipb-map import --format bsp \
  --input /path/to/cstrike/maps/de_dust2.bsp \
  --output ./results/de_dust2.aipbmap
```

The output directory must not exist.
The importer reads the 15 BSP lump descriptors, builds a separate mesh for the world and each brush model with faces, parses entity key/value pairs, records their brush-model references and origin, and writes a versioned map artifact.
The source SHA-256 is computed from the original BSP byte content.
The output contains render face geometry, **not collision hulls**; it is not yet suitable for determining player reachability.

Unsupported data such as clipnodes, BSP collision hulls, face textures, visibility, lighting and dynamic object simulation are intentionally deferred.
Entity properties with duplicate keys are rejected rather than silently discarded because `MapModel` currently stores a string-keyed map.
Entity strings must be valid UTF-8 for the D203 JSON writer; legacy non-UTF-8 encodings require a future explicit import policy.
Only GoldSrc BSP version 30 is supported; malformed source files fail with an error rather than producing a partial artifact.
See [D204 BSP importer notes](../../docs/ai/INDEPENDENT_AI_BSP_IMPORT.md).

## Architecture

- `aipb_map_core`: STL-only geometry, map/navigation models, structural validation and shared SHA-256.
- `aipb_map_io`: JSON/JSONL serialization, artifact manifests and file integrity checks.
- `aipb_map_bsp`: GoldSrc BSP v30 reader and `IMapImporter` implementation.
- `aipb-map`: CLI integration, using the importer and artifact writer.

There is no dependency on the repository root Meson/CMake build, the YaPB DLL, crlib or the GoldSrc SDK.
External constraints (such as YaPB waypoint/link limits) will be handled only by the future exporter.
Project-specific coding rules are in [AGENTS.md](AGENTS.md).
