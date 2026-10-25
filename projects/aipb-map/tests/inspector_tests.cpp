#include <aipb/map/analysis/inspector.h>
#include <aipb/map/analysis/report_json.h>
#include <aipb/map/io/artifact.h>

#include "json.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using namespace aipb::map;
using namespace aipb::map::analysis;

void require(
  bool ok,
  const char *why
) {
  if (!ok) {
    throw std::runtime_error(why);
  }
}

bool hasFinding(
  const MapReport &report,
  std::string_view code
) {
  return std::ranges::any_of(report.findings, [code](const Finding &finding) { return finding.code == code; });
}

MapModel fixture() {
  MapModel map {
    .name = "inspection_fixture",
    .sourceFormat = "synthetic",
    .sourceSha256 = std::string(64, 'a'),
    .units = "goldsrc_units",
    .meshes = {
      { 1, { { 0, 0, 0 }, { 2, 0, 0 }, { 0, 2, 0 }, { 1, 1, 0 }, { 5, 0, 0 } }, { { 0, 1, 2 }, { 0, 1, 4 } } }
    },
    .collision = {
      { 17, "triangle", { { -2, 0, -3 }, { -1, 0, -3 }, { -1, 1, -3 } }, { { 0, 1, 2 } } }
    },
    .entities = {
      { 4, "worldspawn", { 0, 0, 0 }, {}, 1, {}, 7 },
      { 5, "func_door", { 1, 2, 3 }, {}, {}, {}, 8 }
    },
    .partition = CollisionPartition {
      .planes = { { { 0, 0, 1 }, 0, 2 } },
      .renderNodes = { { 0, 1, -1 }, { 0, -2, -2 } },
      .clipNodes = { { 0, 1, -1 }, { 0, -2, -3 }, { 0, -4, -1 } },
      .leaves = { { -2 }, { -1 }, { -3 } },
      .models = {
        { 7, { -10, -20, -30 }, { 10, 20, 30 }, {}, { 0, 0, -2, -1 } },
        { 8, { -5, -5, -5 }, { 5, 5, 5 }, {}, { -1, 1, -2, -1 } }
      }
    }
  };
  validate(map);

  return map;
}

void testReport() {
  const auto map = fixture();
  const auto original = map;
  const auto report = inspect(map, std::string(64, 'b'));
  require(map == original, "inspection modified MapModel");
  require(report.mapName == map.name && report.artifactSha256 == std::string(64, 'b'), "provenance absent");
  require(report.meshCount == 1 && report.vertexCount == 5 && report.triangleCount == 2, "render counts");
  require(report.zeroAreaTriangles == 1 && report.unusedVertices == 1, "mesh diagnostics");
  require(report.collisionShapeCount == 1 && report.collisionTriangleCount == 1, "collision shape metrics");
  require(report.entityCount == 2 && report.entityTypes.at("func_door") == 1, "entity classes");
  require(report.renderBounds.available && report.renderBounds.minimum == Vec3 { 0, 0, 0 } &&
              report.renderBounds.maximum == Vec3 { 5, 2, 0 },
          "render bounds");
  require(report.collisionBounds.available && report.collisionBounds.minimum == Vec3 { -2, 0, -3 } &&
              report.collisionBounds.maximum == Vec3 { -1, 1, -3 },
          "collision shape bounds");
  require(report.partition.sourceModelBounds.available && report.partition.sourceModelBounds.minimum == Vec3 { -10, -20, -30 } &&
              report.partition.sourceModelBounds.maximum == Vec3 { 10, 20, 30 },
          "raw partition model bounds");
  require(report.partition.available && report.partition.models == 2 && report.partition.planes == 1, "partition counts");
  require(report.partition.hulls.size() == 8 && report.partition.hulls[0].reachableBranches == 2 &&
              report.partition.hulls[0].distinctTerminals == 2 && report.partition.hulls[0].maximumDepth == 2,
          "hull0 traversal");
  require(report.partition.hulls[1].reachableBranches == 2 && report.partition.hulls[1].distinctTerminals == 3 &&
              report.partition.hulls[1].maximumDepth == 2,
          "hull1 traversal");
  require(report.partition.hulls[4].maximumDepth == 0 && report.partition.hulls[4].reachableBranches == 0, "negative hull root");
  require(report.partition.unreachableRenderNodes == 0 && report.partition.unreachableClipNodes == 1 &&
              report.partition.unreachableLeaves == 1,
          "unreferenced partition metrics");
  require(report.partition.leafContents.at(-3) == 1, "leaf contents histogram");
  require(hasFinding(report, "zero_area_triangles") && hasFinding(report, "unused_mesh_vertices") &&
              hasFinding(report, "unreferenced_partition_records"),
          "inspection findings missing");
  require(!hasFinding(report, "missing_collision_partition"), "false missing partition warning");
  const auto json = toJson(report);
  const auto data = io::parseJson(json);
  require(data.at("format").text() == "aipb-map-inspection", "report format");
  require(data.at("schema_version").text() == "0.1.0", "report schema");
  require(data.at("mesh_count").number() == 1 && data.at("partition").at("hulls").array().size() == 8, "JSON content mismatch");
  require(data.at("meshes").array()[0].at("id").text() == "1", "report identifier not string");
  require(toJson(report) == json && json.back() == '\n', "report JSON not deterministic");
}

void testWithoutPartition() {
  auto map = fixture();
  map.partition.reset();
  map.entities[0].partitionModelId.reset();
  map.entities[1].partitionModelId.reset();
  map.meshes.clear();
  map.entities[0].meshId.reset();
  const auto report = inspect(map);
  require(!report.partition.available && !report.renderBounds.available, "missing data inferred");
  require(hasFinding(report, "missing_collision_partition") && hasFinding(report, "empty_render_geometry"),
          "missing-data findings missing");
  const auto json = io::parseJson(toJson(report));
  require(json.at("partition").isNull() && json.at("render_bounds").isNull(), "missing bounds not null");
}

void testLongHullAndArtifact() {
  auto map = fixture();
  constexpr std::size_t n = 2048;
  auto &part = *map.partition;
  part.clipNodes.clear();

  for (std::size_t i = 0; i < n; ++i) {
    part.clipNodes.push_back({ 0, i + 1 == n ? -1 : static_cast<std::int32_t>(i + 1), -2 });
  }
  part.models[0].headNodes = { 0, 0, -1, -1 };
  part.models[1].headNodes = { -1, 0, -1, -1 };
  validate(map);
  const auto report = inspect(map);
  require(report.partition.hulls[1].maximumDepth == n, "iterative deep traversal depth");
  require(report.partition.hulls[1].reachableBranches == n, "deep traversal reachability");
  const auto base = std::filesystem::temp_directory_path() / "aipb_map_d206_inspector_tests";
  std::filesystem::remove_all(base);
  std::filesystem::create_directories(base);

  try {
    const auto destination = base / "fixture.aipbmap";
    const auto digest = io::saveMap(map, destination);
    const auto artifact = io::loadMap(destination);
    const auto rereport = inspect(artifact.model, artifact.sha256);
    require(artifact.sha256 == digest && rereport.partition.hulls[1].maximumDepth == n, "artifact inspection roundtrip");
    require(artifact.model == map, "inspect path mutated artifact");
  } catch (...) {
    std::filesystem::remove_all(base);
    throw;
  }
  std::filesystem::remove_all(base);
}

} // namespace

int main(
  int argc,
  char *argv[]
) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--write-fixture") {
      io::saveMap(fixture(), argv[2]);

      return 0;
    }
    require(argc == 1, "unknown inspector test argument");
    testReport();
    testWithoutPartition();
    testLongHullAndArtifact();
    std::cout << "D206 map inspection checks passed\n";

    return 0;
  } catch (const std::exception &error) {
    std::cerr << "D206 map inspection: " << error.what() << '\n';

    return 1;
  }
}
