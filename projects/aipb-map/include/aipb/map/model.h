#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aipb::map {

using ObjectId = std::uint64_t;

struct Vec3 {
    double x {};
    double y {};
    double z {};
    bool operator==(const Vec3&) const = default;
};

struct Triangle {
    std::uint32_t a {};
    std::uint32_t b {};
    std::uint32_t c {};
    bool operator==(const Triangle&) const = default;
};

struct Mesh {
    ObjectId id {};
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
    bool operator==(const Mesh&) const = default;
};

// Collision geometry is independent of visual meshes and runtime reachability.
struct CollisionShape {
    ObjectId id {};
    std::string kind;
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
    bool operator==(const CollisionShape&) const = default;
};

struct MapEntity {
    ObjectId id {};
    std::string type;
    Vec3 origin {};
    std::map<std::string, std::string> properties;
    std::optional<ObjectId> meshId;
    std::optional<ObjectId> collisionId;
    bool operator==(const MapEntity&) const = default;
};

struct MapModel {
    std::string name;
    std::string sourceFormat;
    std::string sourceSha256;
    std::string units;
    std::vector<Mesh> meshes;
    std::vector<CollisionShape> collision;
    std::vector<MapEntity> entities;
    bool operator==(const MapModel&) const = default;
};

struct NavigationRegion {
    ObjectId id {};
    std::vector<ObjectId> meshIds;
    bool operator==(const NavigationRegion&) const = default;
};

struct NavigationNode {
    ObjectId id {};
    Vec3 position {};
    std::optional<ObjectId> regionId;
    std::string role;
    bool operator==(const NavigationNode&) const = default;
};

struct NavigationEdge {
    ObjectId id {};
    ObjectId from {};
    ObjectId to {};
    std::string traversal;
    std::map<std::string, double> parameters;
    bool operator==(const NavigationEdge&) const = default;
};

struct NavigationModel {
    // Content digest supplied by the artifact writer for the corresponding MapModel.
    std::string mapArtifactSha256;
    std::vector<NavigationRegion> regions;
    std::vector<NavigationNode> nodes;
    std::vector<NavigationEdge> edges;
    bool operator==(const NavigationModel&) const = default;
};

// Structural validation does not judge physical reachability or gameplay quality.
void validate(const MapModel& model);
void validate(const NavigationModel& model, const MapModel* map = nullptr);

} // namespace aipb::map
