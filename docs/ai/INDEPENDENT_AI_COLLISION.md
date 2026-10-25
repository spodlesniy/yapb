# D205 — GoldSrc collision partitions and map schema v0.2.0

## Purpose and boundary

D205 imports GoldSrc BSP v30 collision partition data into independent STL-only `MapModel` structures.
The data is geometry and tree topology, not an assertion that a player can traverse a path.
The implementation remains within `projects/aipb-map/`; it neither changes YaPB runtime behavior nor adds crlib or GoldSrc SDK dependencies.

## Representation

`MapModel.partition` is optional and contains a `CollisionPartition` with five ordered collections.

- `planes`: oriented plane normal, signed distance and source orientation-type integer (GoldSrc types 0–5).
- `renderNodes`: partition branches used by hull 0; nonnegative children are node indices, negative children encode `-(leafIndex + 1)`.
- `clipNodes`: partition branches used by clip hulls 1–3; nonnegative children are clipnode indices, negative children are signed GoldSrc CONTENTS codes.
- `leaves`: indexed signed contents values used by hull 0.
- `models`: one per BSP world/brush model, with an object ID, original bounds and origin and four signed hull root references.

Indexing is intentionally zero-based within each partition array and is not a persistent cross-run navigation ID.
`CollisionPartitionModel.id` is the source model ordinal plus one; the `MapEntity.partitionModelId` optional reference associates worldspawn and brush entities with their collision model.
A model need not have polygonal collision triangles; the preexisting `CollisionShape` collection remains independent for other importers.
The import uses model bounds and origin without applying entity motion or translating the raw partition planes.
Negative leaf contents and negative clip contents have **different meanings** and are not converted into each other.

## BSP parsing and structural integrity

Import reads the native BSP planes (lump 1, 20-byte records), render nodes (lump 5, 24-byte records), clipnodes (lump 9, 8-byte records), leaves (lump 10, 28-byte records), and four model headnodes (lump 14, 64-byte records).
Signed 16-bit children, signed 32-bit contents/roots, plane references and float32 coordinate values are decoded explicitly as little-endian data.
The parser checks record alignment, plane type, finite coordinates, model bounds, branch/leaf/root reference ranges, malformed contents and cycles in both node collections.
When all four partition lumps are absent, the optional partition is missing rather than filled with guessed colliders; this retains support for geometry-only test BSPs.
If partition data exists but references are incomplete or invalid, import fails rather than silently degrading to visual geometry.

The collision representation preserves the splitting-plane tests and contents connectivity, but it is **not** a standalone GoldSrc trace engine.
It does not implement hull extents, trace movement, dynamic doors/lifts, engine-side contents exceptions, textures or liquids' gameplay consequences.
For authoritative movement and navigation tests, the future analyzer must apply an explicit actor/physics profile and validate behavior against GoldSrc gameplay.

## Map artifact v0.2.0

`.aipbmap` serialization now writes map schema `0.2.0`, retaining the D203 `manifest.json`, `meshes.jsonl`, `collision.jsonl` and `entities.jsonl` files.
It adds `partition_planes.jsonl`, `partition_nodes.jsonl`, `partition_clipnodes.jsonl`, `partition_leaves.jsonl` and `partition_models.jsonl`.
All new streams are covered by manifest SHA-256 hashes and the canonical artifact digest.
The `has_partition` manifest flag distinguishes absence from an empty set of streams; the model requires at least one model whenever a partition exists.
`MapEntity` JSON adds nullable `partition_model_id`, with 64-bit IDs serialized as decimal strings.

`loadMap()` continues to read D203 map schema `0.1.0` without these additional streams or entity references; its old artifact fingerprint still identifies the original canonical bytes.
Re-saving an old map writes v0.2.0 and yields a new artifact fingerprint; downstream navigation artifacts referring to the old hash must not be silently rebound.
The `.aipbnav` schema is unchanged at `0.1.0`.

## Verification and limits

Focused synthetic BSP tests cover world/brush collision models, positive and negative child meanings, leaf contents, intact hull roots and export/import of v0.2.0 artifacts.
Invalid plane indices/types, tree cycles, incorrect leaf and clipnode references, invalid hull roots, truncated records, non-finite planes, dangling entity links and tampered streams are rejected.
A separate test confirms reading a minimal D203 v0.1.0 artifact.
Run standalone CMake/CTest (Debug and Release) and optionally sanitizers.
Real Counter-Strike maps and in-game collision behavior still need acceptance testing; no claim of physical path correctness is made by D205.

The BSP record layout and contents-code meanings follow [Valve's public GoldSrc BSP declarations](https://github.com/ValveSoftware/halflife/blob/master/utils/common/bspfile.h), consulted read-only.
