#include <aipb/map/model.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace aipb::map {
namespace {

void require(
  bool condition,
  std::string_view message
) {
  if (!condition) {
    throw std::invalid_argument(std::string(message));
  }
}

bool finite(Vec3 point) {
  return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

void checkTriangles(
  std::span<const Vec3> vertices,
  std::span<const Triangle> triangles
) {
  for (const auto &vertex : vertices) {
    require(finite(vertex), "mesh contains a non-finite coordinate");
  }

  for (const auto &triangle : triangles) {
    require(triangle.a < vertices.size() && triangle.b < vertices.size() && triangle.c < vertices.size(),
            "triangle references a missing vertex");
    require(triangle.a != triangle.b && triangle.b != triangle.c && triangle.a != triangle.c, "triangle repeats a vertex");
  }
}

template <typename Collection> std::set<ObjectId> uniqueIds(const Collection &items) {
  std::set<ObjectId> ids;

  for (const auto &item : items) {
    require(item.id != 0, "object ID must be nonzero");
    require(ids.insert(item.id).second, "duplicate object ID in collection");
  }

  return ids;
}

bool sha256(std::string_view hash) {
  return hash.size() == 64 && std::ranges::all_of(hash, [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}

void checkBranchGraph(
  const std::vector<CollisionBranch> &branches,
  std::size_t planes,
  std::size_t leaves,
  bool indexedLeaves
) {
  std::vector<std::uint8_t> state(branches.size(), 0);

  for (std::size_t start = 0; start < branches.size(); ++start) {
    if (state[start] != 0) {
      continue;
    }
    std::vector<std::pair<std::size_t, bool>> pending { { start, false } };

    while (!pending.empty()) {
      const auto [index, finish] = pending.back();
      pending.pop_back();

      if (finish) {
        state[index] = 2;
        continue;
      }

      if (state[index] == 2) {
        continue;
      }
      require(state[index] != 1, "collision tree contains a cycle");
      state[index] = 1;
      const auto &node = branches[index];
      require(node.planeIndex < planes, "collision branch references missing plane");
      pending.emplace_back(index, true);

      for (const auto child : { node.front, node.back }) {
        if (child >= 0) {
          const auto next = static_cast<std::size_t>(child);
          require(next < branches.size(), "collision branch references missing node");

          if (state[next] == 1) {
            throw std::invalid_argument("collision tree contains a cycle");
          }

          if (state[next] == 0) {
            pending.emplace_back(next, false);
          }
        } else if (indexedLeaves) {
          const auto leaf = -static_cast<std::int64_t>(child) - 1;
          require(static_cast<std::uint64_t>(leaf) < leaves, "collision branch references missing leaf");
        }
      }
    }
  }
}

void checkPartition(const CollisionPartition &partition) {
  require(!partition.models.empty(), "collision partition has no models");

  for (const auto &plane : partition.planes) {
    require(finite(plane.normal) && std::isfinite(plane.distance), "collision plane has non-finite geometry");
  }

  for (const auto &leaf : partition.leaves) {
    require(leaf.contents < 0, "collision leaf has invalid contents");
  }
  checkBranchGraph(partition.renderNodes, partition.planes.size(), partition.leaves.size(), true);
  checkBranchGraph(partition.clipNodes, partition.planes.size(), 0, false);
  uniqueIds(partition.models);

  for (const auto &model : partition.models) {
    require(finite(model.mins) && finite(model.maxs) && finite(model.origin), "collision model has invalid coordinates");
    require(model.mins.x <= model.maxs.x && model.mins.y <= model.maxs.y && model.mins.z <= model.maxs.z,
            "collision model bounds are inverted");

    for (std::size_t hull = 0; hull < model.headNodes.size(); ++hull) {
      const auto head = model.headNodes[hull];

      if (head >= 0) {
        require(static_cast<std::size_t>(head) < (hull == 0 ? partition.renderNodes.size() : partition.clipNodes.size()),
                "collision model references missing hull root");
      } else if (hull == 0) {
        const auto leaf = -static_cast<std::int64_t>(head) - 1;
        require(static_cast<std::uint64_t>(leaf) < partition.leaves.size(), "collision model references missing root leaf");
      }
    }
  }
}

} // namespace

void validate(const MapModel &model) {
  require(!model.name.empty(), "map name is empty");
  require(!model.sourceFormat.empty(), "source format is empty");
  require(sha256(model.sourceSha256), "source SHA-256 must be 64 lowercase hexadecimal characters");
  require(!model.units.empty(), "map coordinate units are empty");
  const auto meshIds = uniqueIds(model.meshes);
  const auto collisionIds = uniqueIds(model.collision);
  uniqueIds(model.entities);
  std::set<ObjectId> partitionIds;

  if (model.partition) {
    checkPartition(*model.partition);
    partitionIds = uniqueIds(model.partition->models);
  }

  for (const auto &mesh : model.meshes) {
    checkTriangles(mesh.vertices, mesh.triangles);
  }

  for (const auto &shape : model.collision) {
    require(!shape.kind.empty(), "collision shape kind is empty");
    checkTriangles(shape.vertices, shape.triangles);
  }

  for (const auto &entity : model.entities) {
    require(!entity.type.empty(), "entity type is empty");

    if (!entity.sourceProperties.empty()) {
      std::map<std::string, std::string> effective;

      for (const auto &[key, value] : entity.sourceProperties) {
        effective.insert_or_assign(key, value);
      }

      require(effective == entity.properties, "entity source properties disagree with effective properties");
    }
    require(finite(entity.origin), "entity origin is non-finite");
    require(!entity.meshId || meshIds.contains(*entity.meshId), "entity references missing mesh");
    require(!entity.collisionId || collisionIds.contains(*entity.collisionId), "entity references missing collision");
    require(!entity.partitionModelId || partitionIds.contains(*entity.partitionModelId),
            "entity references missing collision partition model");
  }
}

void validate(
  const NavigationModel &model,
  const MapModel *map
) {
  require(sha256(model.mapArtifactSha256), "map artifact SHA-256 must be 64 lowercase hexadecimal characters");
  const auto regionIds = uniqueIds(model.regions);
  const auto nodeIds = uniqueIds(model.nodes);
  uniqueIds(model.edges);
  std::set<ObjectId> meshIds;

  if (map) {
    validate(*map);
    meshIds = uniqueIds(map->meshes);
  }

  for (const auto &region : model.regions) {
    std::set<ObjectId> seen;

    for (const ObjectId id : region.meshIds) {
      require(id != 0, "region has a zero mesh reference");
      require(seen.insert(id).second, "region repeats a mesh reference");
      require(!map || meshIds.contains(id), "region references missing map mesh");
    }
  }

  for (const auto &node : model.nodes) {
    require(finite(node.position), "node coordinate is non-finite");
    require(!node.regionId || regionIds.contains(*node.regionId), "node references missing region");
  }

  for (const auto &edge : model.edges) {
    require(nodeIds.contains(edge.from) && nodeIds.contains(edge.to), "edge references missing node");
    require(edge.from != edge.to, "edge loops to the same node");
    require(!edge.traversal.empty(), "edge traversal is empty");

    for (const auto &[key, value] : edge.parameters) {
      require(!key.empty() && std::isfinite(value), "invalid traversal parameter");
    }
  }
}

} // namespace aipb::map
