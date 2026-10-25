#pragma once

#include <aipb/map/model.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace aipb::map::analysis {

struct Bounds {
    bool available {};
    Vec3 minimum {};
    Vec3 maximum {};
};

struct MeshMetrics {
    ObjectId id {};
    std::size_t vertices {};
    std::size_t triangles {};
    std::size_t unusedVertices {};
    std::size_t zeroAreaTriangles {};
    Bounds bounds {};
};

struct HullMetrics {
    ObjectId modelId {};
    std::size_t hull {};
    std::int32_t root {};
    std::size_t reachableBranches {};
    std::size_t distinctTerminals {};
    std::size_t maximumDepth {};
};

struct PartitionMetrics {
    bool available {};
    Bounds sourceModelBounds {};
    std::size_t planes {};
    std::size_t renderNodes {};
    std::size_t clipNodes {};
    std::size_t leaves {};
    std::size_t models {};
    std::size_t unreachableRenderNodes {};
    std::size_t unreachableClipNodes {};
    std::size_t unreachableLeaves {};
    std::map<std::int32_t, std::size_t> leafContents;
    std::vector<HullMetrics> hulls;
};

struct Finding {
    std::string severity;
    std::string code;
    std::string detail;
};

struct MapReport {
    std::string mapName;
    std::string sourceFormat;
    std::string sourceSha256;
    std::string artifactSha256;
    std::string units;
    Bounds renderBounds {};
    Bounds collisionBounds {};
    std::size_t meshCount {};
    std::size_t vertexCount {};
    std::size_t triangleCount {};
    std::size_t collisionShapeCount {};
    std::size_t collisionTriangleCount {};
    std::size_t entityCount {};
    std::size_t zeroAreaTriangles {};
    std::size_t unusedVertices {};
    std::map<std::string, std::size_t> entityTypes;
    std::vector<MeshMetrics> meshes;
    PartitionMetrics partition;
    std::vector<Finding> findings;
};

// Report geometry and graph topology without mutating the model or proving gameplay reachability.
MapReport inspect(const MapModel& model, std::string artifactSha256 = {});

} // namespace aipb::map::analysis
