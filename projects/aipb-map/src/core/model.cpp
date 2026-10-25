#include <aipb/map/model.h>

#include <cmath>
#include <set>
#include <stdexcept>
#include <string_view>

namespace aipb::map {
namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::invalid_argument(std::string(message));
    }
}

bool finite(Vec3 point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

void checkTriangles(const std::vector<Vec3>& vertices, const std::vector<Triangle>& triangles) {
    for (const auto& vertex : vertices) {
        require(finite(vertex), "mesh contains a non-finite coordinate");
    }
    for (const auto& triangle : triangles) {
        require(triangle.a < vertices.size() && triangle.b < vertices.size() &&
                    triangle.c < vertices.size(), "triangle references a missing vertex");
        require(triangle.a != triangle.b && triangle.b != triangle.c &&
                    triangle.a != triangle.c, "triangle repeats a vertex");
    }
}

template <typename Collection>
std::set<ObjectId> uniqueIds(const Collection& items) {
    std::set<ObjectId> ids;
    for (const auto& item : items) {
        require(item.id != 0, "object ID must be nonzero");
        require(ids.insert(item.id).second, "duplicate object ID in collection");
    }
    return ids;
}

bool sha256(const std::string& hash) {
    if (hash.size() != 64) {
        return false;
    }
    for (const char c : hash) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

} // namespace

void validate(const MapModel& model) {
    require(!model.name.empty(), "map name is empty");
    require(!model.sourceFormat.empty(), "source format is empty");
    require(sha256(model.sourceSha256), "source SHA-256 must be 64 lowercase hexadecimal characters");
    require(!model.units.empty(), "map coordinate units are empty");
    const auto meshIds = uniqueIds(model.meshes);
    const auto collisionIds = uniqueIds(model.collision);
    uniqueIds(model.entities);
    for (const auto& mesh : model.meshes) {
        checkTriangles(mesh.vertices, mesh.triangles);
    }
    for (const auto& shape : model.collision) {
        require(!shape.kind.empty(), "collision shape kind is empty");
        checkTriangles(shape.vertices, shape.triangles);
    }
    for (const auto& entity : model.entities) {
        require(!entity.type.empty(), "entity type is empty");
        require(finite(entity.origin), "entity origin is non-finite");
        require(!entity.meshId || meshIds.contains(*entity.meshId), "entity references missing mesh");
        require(!entity.collisionId || collisionIds.contains(*entity.collisionId), "entity references missing collision");
    }
}

void validate(const NavigationModel& model, const MapModel* map) {
    require(sha256(model.mapArtifactSha256), "map artifact SHA-256 must be 64 lowercase hexadecimal characters");
    const auto regionIds = uniqueIds(model.regions);
    const auto nodeIds = uniqueIds(model.nodes);
    uniqueIds(model.edges);
    std::set<ObjectId> meshIds;
    if (map) {
        validate(*map);
        meshIds = uniqueIds(map->meshes);
    }
    for (const auto& region : model.regions) {
        std::set<ObjectId> seen;
        for (const ObjectId id : region.meshIds) {
            require(id != 0, "region has a zero mesh reference");
            require(seen.insert(id).second, "region repeats a mesh reference");
            require(!map || meshIds.contains(id), "region references missing map mesh");
        }
    }
    for (const auto& node : model.nodes) {
        require(finite(node.position), "node coordinate is non-finite");
        require(!node.regionId || regionIds.contains(*node.regionId), "node references missing region");
    }
    for (const auto& edge : model.edges) {
        require(nodeIds.contains(edge.from) && nodeIds.contains(edge.to), "edge references missing node");
        require(edge.from != edge.to, "edge loops to the same node");
        require(!edge.traversal.empty(), "edge traversal is empty");
        for (const auto& [key, value] : edge.parameters) {
            require(!key.empty() && std::isfinite(value), "invalid traversal parameter");
        }
    }
}

} // namespace aipb::map
