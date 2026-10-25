#include <aipb/map/analysis/report_json.h>

#include "json.h"

#include <cstdint>
#include <string>
#include <utility>

namespace aipb::map::analysis {
namespace {

using io::Json;
using Object = Json::Object;
using Array = Json::Array;

Json number(std::size_t count) {
  return Json(static_cast<double>(count));
}
Json encode(const Vec3 &point) {
  return Array { point.x, point.y, point.z };
}
Json encode(const Bounds &bounds) {
  if (!bounds.available)
    return Json();
  return Object { { "min", encode(bounds.minimum) }, { "max", encode(bounds.maximum) } };
}
Json encode(const MeshMetrics &metric) {
  return Object { { "id", std::to_string(metric.id) },
                  { "vertices", number(metric.vertices) },
                  { "triangles", number(metric.triangles) },
                  { "unused_vertices", number(metric.unusedVertices) },
                  { "zero_area_triangles", number(metric.zeroAreaTriangles) },
                  { "bounds", encode(metric.bounds) } };
}
Json encode(const HullMetrics &metric) {
  return Object { { "model_id", std::to_string(metric.modelId) },
                  { "hull", number(metric.hull) },
                  { "root", static_cast<double>(metric.root) },
                  { "reachable_branches", number(metric.reachableBranches) },
                  { "distinct_terminals", number(metric.distinctTerminals) },
                  { "maximum_depth", number(metric.maximumDepth) } };
}
Json encode(const Finding &finding) {
  return Object { { "severity", finding.severity }, { "code", finding.code }, { "detail", finding.detail } };
}
Json encode(const PartitionMetrics &p) {
  if (!p.available)
    return Json();
  Array hulls;
  for (const auto &hull : p.hulls)
    hulls.push_back(encode(hull));
  Object leafContents;
  for (const auto &[contents, count] : p.leafContents) {
    leafContents.emplace(std::to_string(contents), number(count));
  }
  return Object { { "source_model_bounds", encode(p.sourceModelBounds) },
                  { "planes", number(p.planes) },
                  { "render_nodes", number(p.renderNodes) },
                  { "clip_nodes", number(p.clipNodes) },
                  { "leaves", number(p.leaves) },
                  { "models", number(p.models) },
                  { "unreachable_render_nodes", number(p.unreachableRenderNodes) },
                  { "unreachable_clip_nodes", number(p.unreachableClipNodes) },
                  { "unreachable_leaves", number(p.unreachableLeaves) },
                  { "leaf_contents", std::move(leafContents) },
                  { "hulls", std::move(hulls) } };
}

} // namespace

std::string toJson(const MapReport &report) {
  Array meshes;
  for (const auto &mesh : report.meshes)
    meshes.push_back(encode(mesh));
  Array findings;
  for (const auto &finding : report.findings)
    findings.push_back(encode(finding));
  Object entityTypes;
  for (const auto &[type, count] : report.entityTypes)
    entityTypes.emplace(type, number(count));
  return io::writeJson(Object { { "format", "aipb-map-inspection" },
                                { "schema_version", "0.1.0" },
                                { "map_name", report.mapName },
                                { "source_format", report.sourceFormat },
                                { "source_sha256", report.sourceSha256 },
                                { "artifact_sha256", report.artifactSha256 },
                                { "units", report.units },
                                { "render_bounds", encode(report.renderBounds) },
                                { "collision_bounds", encode(report.collisionBounds) },
                                { "meshes", std::move(meshes) },
                                { "mesh_count", number(report.meshCount) },
                                { "vertex_count", number(report.vertexCount) },
                                { "triangle_count", number(report.triangleCount) },
                                { "collision_shape_count", number(report.collisionShapeCount) },
                                { "collision_triangle_count", number(report.collisionTriangleCount) },
                                { "entity_count", number(report.entityCount) },
                                { "entity_types", std::move(entityTypes) },
                                { "unused_vertices", number(report.unusedVertices) },
                                { "zero_area_triangles", number(report.zeroAreaTriangles) },
                                { "partition", encode(report.partition) },
                                { "findings", std::move(findings) } }) +
         "\n";
}

} // namespace aipb::map::analysis
