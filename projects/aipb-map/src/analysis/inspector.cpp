#include <aipb/map/analysis/inspector.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace aipb::map::analysis {
namespace {

void extend(
  Bounds &bounds,
  const Vec3 &p
) {
  if (!bounds.available) {
    bounds = { true, p, p };

    return;
  }
  bounds.minimum.x = std::min(bounds.minimum.x, p.x);
  bounds.minimum.y = std::min(bounds.minimum.y, p.y);
  bounds.minimum.z = std::min(bounds.minimum.z, p.z);
  bounds.maximum.x = std::max(bounds.maximum.x, p.x);
  bounds.maximum.y = std::max(bounds.maximum.y, p.y);
  bounds.maximum.z = std::max(bounds.maximum.z, p.z);
}

void extend(
  Bounds &target,
  const Bounds &source
) {
  if (!source.available) {
    return;
  }
  extend(target, source.minimum);
  extend(target, source.maximum);
}

bool zeroArea(
  const Vec3 &a,
  const Vec3 &b,
  const Vec3 &c
) {
  const auto x = static_cast<long double>(b.x) - a.x;
  const auto y = static_cast<long double>(b.y) - a.y;
  const auto z = static_cast<long double>(b.z) - a.z;
  const auto u = static_cast<long double>(c.x) - a.x;
  const auto v = static_cast<long double>(c.y) - a.y;
  const auto w = static_cast<long double>(c.z) - a.z;

  return (y * w - z * v) == 0 && (z * u - x * w) == 0 && (x * v - y * u) == 0;
}

MeshMetrics inspectMesh(
  ObjectId id,
  std::span<const Vec3> vertices,
  std::span<const Triangle> triangles
) {
  MeshMetrics result {
    .id = id,
    .vertices = vertices.size(),
    .triangles = triangles.size()
  };
  std::vector<std::uint8_t> used(vertices.size(), 0);

  for (const auto &vertex : vertices) {
    extend(result.bounds, vertex);
  }

  for (const auto &triangle : triangles) {
    used[triangle.a] = used[triangle.b] = used[triangle.c] = 1;

    if (zeroArea(vertices[triangle.a], vertices[triangle.b], vertices[triangle.c])) {
      ++result.zeroAreaTriangles;
    }
  }
  result.unusedVertices = static_cast<std::size_t>(std::ranges::count(used, std::uint8_t { 0 }));

  return result;
}

// Longest path in the validated acyclic partition tree, computed without recursion.
std::size_t maximumDepth(
  const std::vector<CollisionBranch> &branches,
  std::int32_t root,
  std::vector<std::size_t> &memo
) {
  if (root < 0) {
    return 0;
  }
  std::vector<std::pair<std::size_t, bool>> pending { { static_cast<std::size_t>(root), false } };

  while (!pending.empty()) {
    const auto [index, finish] = pending.back();
    pending.pop_back();

    if (memo[index] != 0) {
      continue;
    }
    const auto &branch = branches[index];

    if (!finish) {
      pending.emplace_back(index, true);

      for (const auto child : { branch.front, branch.back }) {
        if (child >= 0 && memo[static_cast<std::size_t>(child)] == 0) {
          pending.emplace_back(static_cast<std::size_t>(child), false);
        }
      }
    } else {
      const auto depth = [&](std::int32_t child) { return child >= 0 ? memo[static_cast<std::size_t>(child)] : std::size_t {}; };
      memo[index] = 1 + std::max(depth(branch.front), depth(branch.back));
    }
  }

  return memo[static_cast<std::size_t>(root)];
}

HullMetrics inspectHull(
  const CollisionPartition &partition,
  ObjectId modelId,
  std::size_t hull,
  std::int32_t root,
  std::vector<std::uint8_t> &globalNodes,
  std::vector<std::uint8_t> &globalLeaves,
  std::vector<std::size_t> &depths
) {
  const auto &branches = hull == 0 ? partition.renderNodes : partition.clipNodes;
  HullMetrics result {
    .modelId = modelId,
    .hull = hull,
    .root = root,
    .maximumDepth = maximumDepth(branches, root, depths)
  };
  std::set<std::int32_t> terminals;
  std::set<std::size_t> visited;
  std::vector<std::int32_t> pending { root };

  while (!pending.empty()) {
    const auto index = pending.back();
    pending.pop_back();

    if (index < 0) {
      terminals.insert(index);

      if (hull == 0) {
        globalLeaves[static_cast<std::size_t>(-static_cast<std::int64_t>(index) - 1)] = 1;
      }
      continue;
    }
    const auto i = static_cast<std::size_t>(index);

    if (!visited.insert(i).second) {
      continue;
    }
    globalNodes[i] = 1;
    pending.push_back(branches[i].front);
    pending.push_back(branches[i].back);
  }
  result.reachableBranches = visited.size();
  result.distinctTerminals = terminals.size();

  return result;
}

PartitionMetrics inspectPartition(const CollisionPartition &partition) {
  PartitionMetrics result {
    .available = true,
    .planes = partition.planes.size(),
    .renderNodes = partition.renderNodes.size(),
    .clipNodes = partition.clipNodes.size(),
    .leaves = partition.leaves.size(),
    .models = partition.models.size(),
    .leafContents = {},
    .hulls = {}
  };

  for (const auto &model : partition.models) {
    extend(result.sourceModelBounds, model.mins);
    extend(result.sourceModelBounds, model.maxs);
  }

  for (const auto &leaf : partition.leaves) {
    ++result.leafContents[leaf.contents];
  }
  std::vector<std::uint8_t> visitedRender(partition.renderNodes.size(), 0);
  std::vector<std::uint8_t> visitedClip(partition.clipNodes.size(), 0);
  std::vector<std::uint8_t> visitedLeaves(partition.leaves.size(), 0);
  std::vector<std::size_t> renderDepths(partition.renderNodes.size(), 0);
  std::vector<std::size_t> clipDepths(partition.clipNodes.size(), 0);

  for (const auto &model : partition.models) {
    for (std::size_t hull = 0; hull < model.headNodes.size(); ++hull) {
      result.hulls.push_back(inspectHull(partition, model.id, hull, model.headNodes[hull], hull == 0 ? visitedRender : visitedClip,
                                         visitedLeaves, hull == 0 ? renderDepths : clipDepths));
    }
  }
  const auto unused = [](const std::vector<std::uint8_t> &visited) {
    return static_cast<std::size_t>(std::ranges::count(visited, std::uint8_t { 0 }));
  };
  result.unreachableRenderNodes = unused(visitedRender);
  result.unreachableClipNodes = unused(visitedClip);
  result.unreachableLeaves = unused(visitedLeaves);

  return result;
}

} // namespace

MapReport inspect(
  const MapModel &model,
  std::string artifactSha256
) {
  validate(model);
  MapReport report {
    .mapName = model.name,
    .sourceFormat = model.sourceFormat,
    .sourceSha256 = model.sourceSha256,
    .artifactSha256 = std::move(artifactSha256),
    .units = model.units,
    .meshCount = model.meshes.size(),
    .collisionShapeCount = model.collision.size(),
    .entityCount = model.entities.size(),
    .entityTypes = {},
    .meshes = {},
    .partition = {},
    .findings = {}
  };

  for (const auto &mesh : model.meshes) {
    auto metrics = inspectMesh(mesh.id, mesh.vertices, mesh.triangles);
    report.vertexCount += metrics.vertices;
    report.triangleCount += metrics.triangles;
    report.unusedVertices += metrics.unusedVertices;
    report.zeroAreaTriangles += metrics.zeroAreaTriangles;
    extend(report.renderBounds, metrics.bounds);

    if (metrics.zeroAreaTriangles != 0) {
      report.findings.push_back(
          { "warning", "zero_area_triangles",
            "mesh " + std::to_string(mesh.id) + " contains " + std::to_string(metrics.zeroAreaTriangles) + " zero-area triangle(s)" });
    }

    if (metrics.unusedVertices != 0) {
      report.findings.push_back(
          { "info", "unused_mesh_vertices",
            "mesh " + std::to_string(mesh.id) + " contains " + std::to_string(metrics.unusedVertices) + " unreferenced vertex/vertices" });
    }
    report.meshes.push_back(std::move(metrics));
  }

  for (const auto &shape : model.collision) {
    report.collisionTriangleCount += shape.triangles.size();

    for (const auto &p : shape.vertices) {
      extend(report.collisionBounds, p);
    }
  }

  for (const auto &entity : model.entities) {
    ++report.entityTypes[entity.type];
  }

  if (!report.renderBounds.available) {
    report.findings.push_back({ "warning", "empty_render_geometry", "map has no render vertices" });
  }

  if (!model.partition) {
    report.findings.push_back(
        { "warning", "missing_collision_partition", "map has no partition trees; physical accessibility cannot be inferred" });
  } else {
    report.partition = inspectPartition(*model.partition);

    if (report.partition.unreachableRenderNodes != 0 || report.partition.unreachableClipNodes != 0 ||
        report.partition.unreachableLeaves != 0) {
      report.findings.push_back(
          { "info", "unreferenced_partition_records", "some partition records are not reachable from any model hull root" });
    }
  }

  return report;
}

} // namespace aipb::map::analysis
