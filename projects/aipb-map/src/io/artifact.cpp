#include <aipb/map/io/artifact.h>

#include "json.h"
#include "sha256.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace aipb::map::io {
namespace {

using Object = Json::Object;
using Array = Json::Array;
using Files = std::map<std::string, std::string>;
constexpr std::size_t kMaxFileBytes = 512u * 1024 * 1024;
constexpr std::size_t kMaxRecordBytes = 16u * 1024 * 1024;

std::string id(ObjectId value) {
  return std::to_string(value);
}

ObjectId asId(const Json &value) {
  const auto &text = value.text();
  ObjectId result = 0;
  const auto conversion = std::from_chars(text.data(), text.data() + text.size(), result);
  if (conversion.ec != std::errc() || conversion.ptr != text.data() + text.size() || !result) {
    throw std::runtime_error("invalid object ID");
  }
  return result;
}

std::uint32_t asIndex(const Json &value) {
  const auto result = value.number();
  if (!std::isfinite(result) || result < 0 || result > std::numeric_limits<std::uint32_t>::max() || std::trunc(result) != result) {
    throw std::runtime_error("invalid triangle vertex index");
  }
  return static_cast<std::uint32_t>(result);
}

Json encode(const Vec3 &p) {
  return Array { p.x, p.y, p.z };
}
Vec3 decodeVec(const Json &value) {
  const auto &a = value.array();
  if (a.size() != 3)
    throw std::runtime_error("expected vec3");
  return { a[0].number(), a[1].number(), a[2].number() };
}
Json encode(const Triangle &t) {
  return Array { static_cast<double>(t.a), static_cast<double>(t.b), static_cast<double>(t.c) };
}
Triangle decodeTriangle(const Json &value) {
  const auto &a = value.array();
  if (a.size() != 3)
    throw std::runtime_error("expected triangle");
  return { asIndex(a[0]), asIndex(a[1]), asIndex(a[2]) };
}

template <typename T, typename Converter> Json encodeList(const std::vector<T> &values, Converter convert) {
  Array result;
  result.reserve(values.size());
  for (const auto &value : values)
    result.push_back(convert(value));
  return Json(std::move(result));
}

template <typename T, typename Converter> std::vector<T> decodeList(const Json &values, Converter convert) {
  std::vector<T> result;
  result.reserve(values.array().size());
  for (const auto &value : values.array())
    result.push_back(convert(value));
  return result;
}

Json encode(const Mesh &m) {
  return Object { { "id", id(m.id) },
                  { "vertices", encodeList(m.vertices, [](const auto &v) { return encode(v); }) },
                  { "triangles", encodeList(m.triangles, [](const auto &t) { return encode(t); }) } };
}
Mesh decodeMesh(const Json &value) {
  return { asId(value.at("id")), decodeList<Vec3>(value.at("vertices"), decodeVec),
           decodeList<Triangle>(value.at("triangles"), decodeTriangle) };
}
Json encode(const CollisionShape &c) {
  return Object { { "id", id(c.id) },
                  { "kind", c.kind },
                  { "vertices", encodeList(c.vertices, [](const auto &v) { return encode(v); }) },
                  { "triangles", encodeList(c.triangles, [](const auto &t) { return encode(t); }) } };
}
CollisionShape decodeCollision(const Json &value) {
  return { asId(value.at("id")), value.at("kind").text(), decodeList<Vec3>(value.at("vertices"), decodeVec),
           decodeList<Triangle>(value.at("triangles"), decodeTriangle) };
}
Json encodeOptional(const std::optional<ObjectId> &value) {
  return value ? Json(id(*value)) : Json();
}
std::optional<ObjectId> decodeOptional(const Json &value) {
  return value.isNull() ? std::nullopt : std::optional<ObjectId> { asId(value) };
}
std::int32_t asSigned32(const Json &value) {
  const auto number = value.number();
  if (!std::isfinite(number) || std::trunc(number) != number || number < std::numeric_limits<std::int32_t>::min() ||
      number > std::numeric_limits<std::int32_t>::max()) {
    throw std::runtime_error("invalid signed collision integer");
  }
  return static_cast<std::int32_t>(number);
}

Json encode(const CollisionPlane &p) {
  return Object { { "normal", encode(p.normal) }, { "distance", p.distance }, { "type", static_cast<double>(p.type) } };
}
CollisionPlane decodePlane(const Json &value) {
  return { decodeVec(value.at("normal")), value.at("distance").number(), asSigned32(value.at("type")) };
}
Json encode(const CollisionBranch &n) {
  return Object { { "plane", static_cast<double>(n.planeIndex) },
                  { "front", static_cast<double>(n.front) },
                  { "back", static_cast<double>(n.back) } };
}
CollisionBranch decodeBranch(const Json &value) {
  return { asIndex(value.at("plane")), asSigned32(value.at("front")), asSigned32(value.at("back")) };
}
Json encode(const CollisionLeaf &leaf) {
  return Object { { "contents", static_cast<double>(leaf.contents) } };
}
CollisionLeaf decodeLeaf(const Json &value) {
  return { asSigned32(value.at("contents")) };
}
Json encode(const CollisionPartitionModel &m) {
  Array heads;
  for (const auto head : m.headNodes)
    heads.emplace_back(static_cast<double>(head));
  return Object {
    { "id", id(m.id) }, { "mins", encode(m.mins) }, { "maxs", encode(m.maxs) }, { "origin", encode(m.origin) }, { "heads", heads }
  };
}
CollisionPartitionModel decodePartitionModel(const Json &value) {
  CollisionPartitionModel model;
  model.id = asId(value.at("id"));
  model.mins = decodeVec(value.at("mins"));
  model.maxs = decodeVec(value.at("maxs"));
  model.origin = decodeVec(value.at("origin"));
  const auto &heads = value.at("heads").array();
  if (heads.size() != model.headNodes.size())
    throw std::runtime_error("expected four collision hull roots");
  for (std::size_t i = 0; i < heads.size(); ++i)
    model.headNodes[i] = asSigned32(heads[i]);
  return model;
}

Json encode(const MapEntity &e) {
  Object fields;
  for (const auto &[key, value] : e.properties)
    fields.emplace(key, value);
  return Object { { "id", id(e.id) },
                  { "type", e.type },
                  { "origin", encode(e.origin) },
                  { "properties", fields },
                  { "mesh_id", encodeOptional(e.meshId) },
                  { "collision_id", encodeOptional(e.collisionId) },
                  { "partition_model_id", encodeOptional(e.partitionModelId) } };
}
MapEntity decodeEntity(const Json &value) {
  MapEntity entity;
  entity.id = asId(value.at("id"));
  entity.type = value.at("type").text();
  entity.origin = decodeVec(value.at("origin"));
  for (const auto &[key, item] : value.at("properties").object()) {
    entity.properties.emplace(key, item.text());
  }
  entity.meshId = decodeOptional(value.at("mesh_id"));
  entity.collisionId = decodeOptional(value.at("collision_id"));
  if (value.object().contains("partition_model_id")) {
    entity.partitionModelId = decodeOptional(value.at("partition_model_id"));
  }
  return entity;
}
Json encode(const NavigationRegion &r) {
  Array ids;
  for (ObjectId value : r.meshIds)
    ids.emplace_back(id(value));
  return Object { { "id", id(r.id) }, { "mesh_ids", ids } };
}
NavigationRegion decodeRegion(const Json &value) {
  return { asId(value.at("id")), decodeList<ObjectId>(value.at("mesh_ids"), asId) };
}
Json encode(const NavigationNode &n) {
  return Object { { "id", id(n.id) }, { "position", encode(n.position) }, { "region_id", encodeOptional(n.regionId) }, { "role", n.role } };
}
NavigationNode decodeNode(const Json &value) {
  return { asId(value.at("id")), decodeVec(value.at("position")), decodeOptional(value.at("region_id")), value.at("role").text() };
}
Json encode(const NavigationEdge &e) {
  Object params;
  for (const auto &[key, value] : e.parameters)
    params.emplace(key, value);
  return Object { { "id", id(e.id) }, { "from", id(e.from) }, { "to", id(e.to) }, { "traversal", e.traversal }, { "parameters", params } };
}
NavigationEdge decodeEdge(const Json &value) {
  NavigationEdge edge;
  edge.id = asId(value.at("id"));
  edge.from = asId(value.at("from"));
  edge.to = asId(value.at("to"));
  edge.traversal = value.at("traversal").text();
  for (const auto &[key, item] : value.at("parameters").object()) {
    edge.parameters.emplace(key, item.number());
  }
  return edge;
}

template <typename Collection> std::string asJsonLines(const Collection &items) {
  std::string content;
  for (const auto &item : items) {
    content += writeJson(encode(item));
    content += '\n';
  }
  return content;
}

template <typename T> void readLines(const std::string &content, std::vector<T> &items, T (*decode)(const Json &)) {
  std::size_t start = 0;
  while (start < content.size()) {
    const auto end = content.find('\n', start);
    if (end == std::string::npos || end - start > kMaxRecordBytes) {
      throw std::runtime_error("invalid or oversized JSONL record");
    }
    items.push_back(decode(parseJson(std::string_view(content).substr(start, end - start))));
    start = end + 1;
  }
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    throw std::runtime_error("unable to open artifact file: " + path.string());
  const auto length = stream.tellg();
  if (length < 0 || static_cast<std::uintmax_t>(length) > kMaxFileBytes) {
    throw std::runtime_error("artifact file exceeds size limit");
  }
  std::string content(static_cast<std::size_t>(length), '\0');
  stream.seekg(0);
  if (!stream.read(content.data(), static_cast<std::streamsize>(content.size()))) {
    throw std::runtime_error("unable to read artifact file");
  }
  return content;
}
void writeFile(const std::filesystem::path &path, const std::string &contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output || !output.write(contents.data(), static_cast<std::streamsize>(contents.size()))) {
    throw std::runtime_error("unable to write artifact file: " + path.string());
  }
}

std::string digest(const Files &files) {
  std::string canonical;
  for (const auto &[name, bytes] : files) {
    canonical += std::to_string(name.size()) + ':' + name;
    canonical += std::to_string(bytes.size()) + ':' + bytes;
  }
  return sha256Hex(canonical);
}

std::string save(const Files &records, const std::filesystem::path &dir) {
  if (std::filesystem::exists(dir))
    throw std::runtime_error("artifact destination already exists");
  if (!std::filesystem::create_directories(dir))
    throw std::runtime_error("unable to create artifact directory");
  try {
    for (const auto &[name, contents] : records)
      writeFile(dir / name, contents);
  } catch (...) {
    std::filesystem::remove_all(dir);
    throw;
  }
  return digest(records);
}

Files read(const std::filesystem::path &dir, const std::vector<std::string> &files) {
  Files result;
  for (const auto &file : files)
    result.emplace(file, readFile(dir / file));
  const auto &manifest = parseJson(result.at("manifest.json"));
  const auto &hashes = manifest.at("file_sha256").object();
  if (hashes.size() != files.size() - 1)
    throw std::runtime_error("unexpected artifact manifest file count");
  for (const auto &file : files) {
    if (file != "manifest.json" && hashes.at(file).text() != sha256Hex(result.at(file))) {
      throw std::runtime_error("artifact stream checksum mismatch: " + file);
    }
  }
  return result;
}

std::string manifest(const std::string &format, Object fields, const Files &streams, std::string_view version = "0.1.0") {
  Object hashes;
  for (const auto &[name, content] : streams)
    hashes.emplace(name, sha256Hex(content));
  fields.emplace("format", format);
  fields.emplace("schema_version", std::string(version));
  fields.emplace("file_sha256", hashes);
  return writeJson(Json(std::move(fields))) + '\n';
}

void checkVersion(const Json &value, const std::string &format) {
  if (value.at("format").text() != format || value.at("schema_version").text() != "0.1.0") {
    throw std::runtime_error("unsupported map artifact schema or format");
  }
}

} // namespace

std::string saveMap(const MapModel &model, const std::filesystem::path &directory) {
  validate(model);
  Files files { { "meshes.jsonl", asJsonLines(model.meshes) },
                { "collision.jsonl", asJsonLines(model.collision) },
                { "entities.jsonl", asJsonLines(model.entities) },
                { "partition_planes.jsonl", "" },
                { "partition_nodes.jsonl", "" },
                { "partition_clipnodes.jsonl", "" },
                { "partition_leaves.jsonl", "" },
                { "partition_models.jsonl", "" } };
  if (model.partition) {
    const auto &p = *model.partition;
    files["partition_planes.jsonl"] = asJsonLines(p.planes);
    files["partition_nodes.jsonl"] = asJsonLines(p.renderNodes);
    files["partition_clipnodes.jsonl"] = asJsonLines(p.clipNodes);
    files["partition_leaves.jsonl"] = asJsonLines(p.leaves);
    files["partition_models.jsonl"] = asJsonLines(p.models);
  }
  files.emplace("manifest.json", manifest("aipb-map",
                                          Object { { "name", model.name },
                                                   { "source_format", model.sourceFormat },
                                                   { "source_sha256", model.sourceSha256 },
                                                   { "units", model.units },
                                                   { "has_partition", model.partition.has_value() } },
                                          files, "0.2.0"));
  return save(files, directory);
}

MapArtifact loadMap(const std::filesystem::path &directory) {
  const auto info = parseJson(readFile(directory / "manifest.json"));
  if (info.at("format").text() != "aipb-map")
    throw std::runtime_error("invalid map artifact format");
  const auto version = info.at("schema_version").text();
  if (version != "0.1.0" && version != "0.2.0") {
    throw std::runtime_error("unsupported map artifact schema");
  }
  std::vector<std::string> required { "manifest.json", "meshes.jsonl", "collision.jsonl", "entities.jsonl" };
  if (version == "0.2.0") {
    for (const auto *name : { "partition_planes.jsonl", "partition_nodes.jsonl", "partition_clipnodes.jsonl", "partition_leaves.jsonl",
                              "partition_models.jsonl" })
      required.emplace_back(name);
  }
  const auto files = read(directory, required);
  MapModel model;
  model.name = info.at("name").text();
  model.sourceFormat = info.at("source_format").text();
  model.sourceSha256 = info.at("source_sha256").text();
  model.units = info.at("units").text();
  readLines(files.at("meshes.jsonl"), model.meshes, decodeMesh);
  readLines(files.at("collision.jsonl"), model.collision, decodeCollision);
  readLines(files.at("entities.jsonl"), model.entities, decodeEntity);
  if (version == "0.2.0") {
    const bool present = std::get<bool>(info.at("has_partition").data);
    CollisionPartition partition;
    readLines(files.at("partition_planes.jsonl"), partition.planes, decodePlane);
    readLines(files.at("partition_nodes.jsonl"), partition.renderNodes, decodeBranch);
    readLines(files.at("partition_clipnodes.jsonl"), partition.clipNodes, decodeBranch);
    readLines(files.at("partition_leaves.jsonl"), partition.leaves, decodeLeaf);
    readLines(files.at("partition_models.jsonl"), partition.models, decodePartitionModel);
    if (present)
      model.partition = std::move(partition);
    else if (!partition.planes.empty() || !partition.renderNodes.empty() || !partition.clipNodes.empty() || !partition.leaves.empty() ||
             !partition.models.empty()) {
      throw std::runtime_error("absent collision partition contains records");
    }
  }
  validate(model);
  return { std::move(model), digest(files) };
}

std::string saveNavigation(const NavigationModel &model, const std::filesystem::path &directory) {
  validate(model);
  Files files { { "regions.jsonl", asJsonLines(model.regions) },
                { "nodes.jsonl", asJsonLines(model.nodes) },
                { "edges.jsonl", asJsonLines(model.edges) } };
  files.emplace("manifest.json", manifest("aipb-navigation", Object { { "map_artifact_sha256", model.mapArtifactSha256 } }, files));
  return save(files, directory);
}

NavigationArtifact loadNavigation(const std::filesystem::path &directory, const MapArtifact *map) {
  const auto files = read(directory, { "manifest.json", "regions.jsonl", "nodes.jsonl", "edges.jsonl" });
  const auto info = parseJson(files.at("manifest.json"));
  checkVersion(info, "aipb-navigation");
  NavigationModel model;
  model.mapArtifactSha256 = info.at("map_artifact_sha256").text();
  readLines(files.at("regions.jsonl"), model.regions, decodeRegion);
  readLines(files.at("nodes.jsonl"), model.nodes, decodeNode);
  readLines(files.at("edges.jsonl"), model.edges, decodeEdge);
  if (map && model.mapArtifactSha256 != map->sha256) {
    throw std::runtime_error("navigation references another map artifact");
  }
  validate(model, map ? &map->model : nullptr);
  return { std::move(model), digest(files) };
}

} // namespace aipb::map::io
