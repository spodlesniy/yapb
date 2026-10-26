#include <aipb/map/hash.h>
#include <aipb/map/importer/bsp_importer.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aipb::map::importer {
namespace {

constexpr std::size_t kLumpCount = 15;
constexpr std::size_t kHeaderBytes = 4 + kLumpCount * 8;
constexpr std::size_t kMaxBspBytes = 512 * 1024 * 1024;
constexpr std::size_t kMaxEntityBytes = 8 * 1024 * 1024;
constexpr std::size_t kMaxEntityTokenBytes = 64 * 1024;
constexpr std::size_t kPlanes = 1;
constexpr std::size_t kNodes = 5;
constexpr std::size_t kClipNodes = 9;
constexpr std::size_t kLeaves = 10;
constexpr std::size_t kVertices = 3;
constexpr std::size_t kFaces = 7;
constexpr std::size_t kEntities = 0;
constexpr std::size_t kEdges = 12;
constexpr std::size_t kSurfEdges = 13;
constexpr std::size_t kModels = 14;

struct Lump {
  std::size_t offset {};
  std::size_t length {};
};

class BspReader {
public:
  explicit BspReader(std::string bytes) : bytes_(std::move(bytes)) {
    if (bytes_.size() < kHeaderBytes) {
      fail("truncated BSP header");
    }

    if (signed32(0) != 30) {
      fail("unsupported BSP version (expected GoldSrc v30)");
    }

    for (std::size_t i = 0; i < kLumpCount; ++i) {
      const auto offset = signed32(4 + i * 8);
      const auto length = signed32(8 + i * 8);

      if (offset < 0 || length < 0) {
        fail("negative BSP lump offset or length");
      }
      const auto start = static_cast<std::size_t>(offset);
      const auto size = static_cast<std::size_t>(length);

      if (start > bytes_.size() || size > bytes_.size() - start || (size != 0 && start < kHeaderBytes)) {
        fail("BSP lump lies outside data or overlaps header");
      }
      lumps_[i] = { start, size };
    }
    checkRecords(kPlanes, 20);
    checkRecords(kNodes, 24);
    checkRecords(kClipNodes, 8);
    checkRecords(kLeaves, 28);
    checkRecords(kVertices, 12);
    checkRecords(kFaces, 20);
    checkRecords(kEdges, 4);
    checkRecords(kSurfEdges, 4);
    checkRecords(kModels, 64);

    if (lumps_[kEntities].length > kMaxEntityBytes) {
      fail("BSP entity lump exceeds limit");
    }
  }

  static BspReader readFile(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);

    if (!input) {
      throw std::runtime_error("cannot open BSP file: " + path.string());
    }
    const auto end = input.tellg();

    if (end < 0 || static_cast<std::uintmax_t>(end) > kMaxBspBytes) {
      throw std::runtime_error("BSP file exceeds size limit");
    }
    std::string data(static_cast<std::size_t>(end), '\0');
    input.seekg(0);

    if (!data.empty() && !input.read(data.data(), static_cast<std::streamsize>(data.size()))) {
      throw std::runtime_error("cannot read BSP file: " + path.string());
    }

    return BspReader(std::move(data));
  }

  std::uint16_t u16(std::size_t offset) const {
    check(offset, 2);
    return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes_[offset])) |
           (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes_[offset + 1])) << 8);
  }
  std::uint32_t u32(std::size_t offset) const {
    check(offset, 4);
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes_[offset])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes_[offset + 1])) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes_[offset + 2])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes_[offset + 3])) << 24);
  }
  std::int16_t signed16(std::size_t offset) const {
    return std::bit_cast<std::int16_t>(u16(offset));
  }
  std::int32_t signed32(std::size_t offset) const {
    return std::bit_cast<std::int32_t>(u32(offset));
  }
  float f32(std::size_t offset) const {
    return std::bit_cast<float>(u32(offset));
  }
  std::size_t count(
    std::size_t index,
    std::size_t recordSize
  ) const {
    return lumps_[index].length / recordSize;
  }
  std::size_t record(
    std::size_t index,
    std::size_t recordSize,
    std::size_t which
  ) const {
    if (which >= count(index, recordSize)) {
      fail("BSP references a missing record");
    }

    return lumps_[index].offset + which * recordSize;
  }
  std::string_view lumpText(std::size_t index) const {
    return std::string_view(bytes_).substr(lumps_[index].offset, lumps_[index].length);
  }
  std::string_view contents() const {
    return bytes_;
  }

private:
  std::string bytes_;
  std::array<Lump, kLumpCount> lumps_ {};

  [[noreturn]] static void fail(const char *message) {
    throw std::runtime_error(message);
  }
  void check(
    std::size_t offset,
    std::size_t size
  ) const {
    if (offset > bytes_.size() || size > bytes_.size() - offset) {
      fail("BSP read out of bounds");
    }
  }
  void checkRecords(
    std::size_t index,
    std::size_t recordSize
  ) const {
    if (lumps_[index].length % recordSize != 0) {
      fail("BSP lump contains a partial record");
    }
  }
};

std::vector<Vec3> vertices(const BspReader &bsp) {
  std::vector<Vec3> result;
  result.reserve(bsp.count(kVertices, 12));

  for (std::size_t i = 0; i < bsp.count(kVertices, 12); ++i) {
    const auto o = bsp.record(kVertices, 12, i);
    const Vec3 point { bsp.f32(o), bsp.f32(o + 4), bsp.f32(o + 8) };

    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
      throw std::runtime_error("BSP has a non-finite vertex");
    }
    result.push_back(point);
  }

  return result;
}

std::vector<Mesh> meshes(
  const BspReader &bsp,
  const BspImporter::WarningHandler &onWarning
) {
  const auto points = vertices(bsp);
  std::vector<Mesh> result;
  const auto modelCount = bsp.count(kModels, 64);

  if (modelCount == 0) {
    throw std::runtime_error("BSP has no world model");
  }

  for (std::size_t modelIndex = 0; modelIndex < modelCount; ++modelIndex) {
    const auto o = bsp.record(kModels, 64, modelIndex);
    const auto first = bsp.signed32(o + 56);
    const auto count = bsp.signed32(o + 60);
    const auto faceCount = bsp.count(kFaces, 20);

    if (first < 0 || count < 0 || static_cast<std::size_t>(first) > faceCount ||
        static_cast<std::size_t>(count) > faceCount - static_cast<std::size_t>(first)) {
      throw std::runtime_error("BSP model references invalid face range");
    }

    if (!count) {
      continue;
    }
    Mesh mesh;
    mesh.id = static_cast<ObjectId>(modelIndex) + 1;
    std::map<std::uint32_t, std::uint32_t> vertexMap;
    auto internVertex = [&](std::uint32_t index) {
      if (index >= points.size()) {
        throw std::runtime_error("BSP face references missing vertex");
      }
      const auto [it, inserted] = vertexMap.emplace(index, static_cast<std::uint32_t>(mesh.vertices.size()));

      if (inserted) {
        mesh.vertices.push_back(points[index]);
      }

      return it->second;
    };

    for (std::size_t face = static_cast<std::size_t>(first); face < static_cast<std::size_t>(first) + static_cast<std::size_t>(count);
         ++face) {
      const auto fo = bsp.record(kFaces, 20, face);
      const auto surfFirst = bsp.signed32(fo + 4);
      const auto surfCount = bsp.u16(fo + 8);
      const auto available = bsp.count(kSurfEdges, 4);

      if (surfFirst < 0 || surfCount < 3 || static_cast<std::size_t>(surfFirst) > available ||
          surfCount > available - static_cast<std::size_t>(surfFirst)) {
        throw std::runtime_error("BSP face references invalid surfedge range");
      }
      std::vector<std::uint32_t> polygon;
      polygon.reserve(surfCount);

      for (std::size_t n = 0; n < surfCount; ++n) {
        const auto signedEdge = bsp.signed32(bsp.record(kSurfEdges, 4, static_cast<std::size_t>(surfFirst) + n));

        if (signedEdge == std::numeric_limits<std::int32_t>::min()) {
          throw std::runtime_error("BSP surfedge has invalid negative index");
        }
        const auto index = static_cast<std::size_t>(signedEdge < 0 ? -signedEdge : signedEdge);
        const auto eo = bsp.record(kEdges, 4, index);
        const auto vertex = bsp.u16(eo + (signedEdge < 0 ? 2 : 0));
        polygon.push_back(internVertex(vertex));
      }

      std::size_t skipped = 0;

      for (std::size_t n = 1; n + 1 < polygon.size(); ++n) {
        const Triangle triangle { polygon.front(), polygon[n], polygon[n + 1] };

        if (triangle.a == triangle.b || triangle.a == triangle.c || triangle.b == triangle.c) {
          ++skipped;
          continue;
        }

        mesh.triangles.push_back(triangle);
      }

      if (skipped > 0 && onWarning) {
        onWarning("BSP model " + std::to_string(modelIndex) + ", face " + std::to_string(face) +
                  ": skipped " + std::to_string(skipped) + " repeated-vertex triangles");
      }
    }
    result.push_back(std::move(mesh));
  }

  return result;
}

std::optional<CollisionPartition> readPartition(const BspReader &bsp) {
  // Geometry-only BSP test fixtures and older imports may have no partition lumps.
  if (!bsp.count(kPlanes, 20) && !bsp.count(kNodes, 24) && !bsp.count(kClipNodes, 8) && !bsp.count(kLeaves, 28)) {
    return std::nullopt;
  }

  CollisionPartition partition;

  for (std::size_t i = 0; i < bsp.count(kPlanes, 20); ++i) {
    const auto o = bsp.record(kPlanes, 20, i);
    const auto type = bsp.signed32(o + 16);

    if (type < 0 || type > 5) {
      throw std::runtime_error("BSP plane has invalid orientation type");
    }
    partition.planes.push_back({ { bsp.f32(o), bsp.f32(o + 4), bsp.f32(o + 8) }, bsp.f32(o + 12), type });
  }

  for (std::size_t i = 0; i < bsp.count(kNodes, 24); ++i) {
    const auto o = bsp.record(kNodes, 24, i);
    const auto plane = bsp.signed32(o);

    if (plane < 0) {
      throw std::runtime_error("negative BSP render-node plane index");
    }
    partition.renderNodes.push_back({ static_cast<std::uint32_t>(plane), bsp.signed16(o + 4), bsp.signed16(o + 6) });
  }

  for (std::size_t i = 0; i < bsp.count(kClipNodes, 8); ++i) {
    const auto o = bsp.record(kClipNodes, 8, i);
    const auto plane = bsp.signed32(o);

    if (plane < 0) {
      throw std::runtime_error("negative BSP clip-node plane index");
    }
    partition.clipNodes.push_back({ static_cast<std::uint32_t>(plane), bsp.signed16(o + 4), bsp.signed16(o + 6) });
  }

  for (std::size_t i = 0; i < bsp.count(kLeaves, 28); ++i) {
    partition.leaves.push_back({ bsp.signed32(bsp.record(kLeaves, 28, i)) });
  }

  for (std::size_t i = 0; i < bsp.count(kModels, 64); ++i) {
    const auto o = bsp.record(kModels, 64, i);
    CollisionPartitionModel model;
    model.id = static_cast<ObjectId>(i) + 1;
    model.mins = { bsp.f32(o), bsp.f32(o + 4), bsp.f32(o + 8) };
    model.maxs = { bsp.f32(o + 12), bsp.f32(o + 16), bsp.f32(o + 20) };
    model.origin = { bsp.f32(o + 24), bsp.f32(o + 28), bsp.f32(o + 32) };

    for (std::size_t h = 0; h < model.headNodes.size(); ++h) {
      model.headNodes[h] = bsp.signed32(o + 36 + 4 * h);
    }
    partition.models.push_back(model);
  }

  return partition;
}

class EntityParser {
public:
  explicit EntityParser(std::string_view contents) : input_(contents) {
  }

  std::vector<MapEntity> parse() {
    std::vector<MapEntity> entities;

    while (true) {
      whitespace();

      if (pos_ == input_.size() || input_[pos_] == '\0') {
        break;
      }
      expect('{');
      MapEntity entity;
      entity.id = static_cast<ObjectId>(entities.size()) + 1;

      while (true) {
        whitespace();

        if (pos_ >= input_.size()) {
          fail("unterminated BSP entity");
        }

        if (input_[pos_] == '}') {
          ++pos_;
          break;
        }
        const auto key = quoted();
        const auto value = quoted();

        entity.sourceProperties.emplace_back(key, value);
        entity.properties.insert_or_assign(key, value);
      }
      const auto type = entity.properties.find("classname");

      if (type == entity.properties.end() || type->second.empty()) {
        fail("BSP entity lacks classname");
      }
      entity.type = type->second;

      if (const auto origin = entity.properties.find("origin"); origin != entity.properties.end()) {
        entity.origin = originValue(origin->second);
      }
      entities.push_back(std::move(entity));
    }

    for (; pos_ < input_.size(); ++pos_) {
      if (input_[pos_] != '\0' && !std::isspace(static_cast<unsigned char>(input_[pos_]))) {
        fail("trailing bytes after BSP entities");
      }
    }

    return entities;
  }

private:
  std::string_view input_;
  std::size_t pos_ {};
  [[noreturn]] static void fail(const char *message) {
    throw std::runtime_error(message);
  }
  void whitespace() {
    while (pos_ < input_.size()) {
      if (std::isspace(static_cast<unsigned char>(input_[pos_]))) {
        ++pos_;
        continue;
      }

      if (pos_ + 1 < input_.size() && input_[pos_] == '/' && input_[pos_ + 1] == '/') {
        pos_ += 2;

        while (pos_ < input_.size() && input_[pos_] != '\n') {
          ++pos_;
        }
        continue;
      }
      break;
    }
  }
  void expect(char c) {
    whitespace();

    if (pos_ >= input_.size() || input_[pos_] != c) {
      fail("invalid BSP entity delimiter");
    }
    ++pos_;
  }
  std::string quoted() {
    whitespace();

    if (pos_ >= input_.size() || input_[pos_] != '"') {
      fail("invalid BSP entity key/value");
    }
    ++pos_;
    const auto start = pos_;
    // GoldSrc entity backslashes are literal, including WAD paths.
    while (pos_ < input_.size() && input_[pos_] != '"' && input_[pos_] != '\0') {
      if (pos_ - start >= kMaxEntityTokenBytes) {
        fail("BSP entity token exceeds limit");
      }
      ++pos_;
    }

    if (pos_ >= input_.size() || input_[pos_] != '"') {
      fail("unterminated BSP entity string");
    }
    const auto value = std::string(input_.substr(start, pos_ - start));
    ++pos_;

    return value;
  }
  static Vec3 originValue(std::string_view input) {
    Vec3 result;
    double *coordinates[3] = { &result.x, &result.y, &result.z };

    for (auto *coordinate : coordinates) {
      while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) {
        input.remove_prefix(1);
      }

      if (input.empty()) {
        fail("invalid BSP origin");
      }
      const auto [end, ec] = std::from_chars(input.data(), input.data() + input.size(), *coordinate);

      if (ec != std::errc {} || end == input.data() || !std::isfinite(*coordinate)) {
        fail("invalid BSP origin coordinate");
      }
      input.remove_prefix(static_cast<std::size_t>(end - input.data()));
    }

    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) {
      input.remove_prefix(1);
    }

    if (!input.empty()) {
      fail("invalid BSP origin suffix");
    }

    return result;
  }
};

void linkEntities(
  const BspReader &bsp,
  const std::vector<Mesh> &meshes,
  const std::optional<CollisionPartition> &partition,
  std::vector<MapEntity> &entities
) {
  const auto models = bsp.count(kModels, 64);

  for (auto &entity : entities) {
    std::size_t number = 0;

    if (const auto m = entity.properties.find("model"); m != entity.properties.end() && !m->second.empty() && m->second.front() == '*') {
      const auto text = std::string_view(m->second).substr(1);
      const auto [end, err] = std::from_chars(text.data(), text.data() + text.size(), number);

      if (err != std::errc {} || end != text.data() + text.size() || number >= models || number == 0) {
        throw std::runtime_error("BSP entity references invalid brush model");
      }
    } else if (entity.type != "worldspawn") {
      continue;
    }
    const auto it =
        std::find_if(meshes.begin(), meshes.end(), [&](const Mesh &mesh) { return mesh.id == static_cast<ObjectId>(number) + 1; });

    if (it == meshes.end()) {
      throw std::runtime_error("BSP entity references empty model geometry");
    }
    entity.meshId = it->id;

    if (partition) {
      entity.partitionModelId = static_cast<ObjectId>(number) + 1;
    }
  }
}

} // namespace

MapModel BspImporter::importFile(const std::filesystem::path &path) const {
  return importWithWarnings(path, {});
}

MapModel BspImporter::importWithWarnings(
  const std::filesystem::path &path,
  const WarningHandler &onWarning
) const {
  const auto bsp = BspReader::readFile(path);
  MapModel map;
  map.name = path.stem().string();
  map.sourceFormat = "goldsrc-bsp-v30";
  map.sourceSha256 = sha256Hex(bsp.contents());
  map.units = "goldsrc_units";
  map.meshes = meshes(bsp, onWarning);
  map.partition = readPartition(bsp);
  map.entities = EntityParser(bsp.lumpText(kEntities)).parse();

  if (map.entities.empty() || map.entities.front().type != "worldspawn") {
    throw std::runtime_error("BSP entity lump must begin with worldspawn");
  }
  linkEntities(bsp, map.meshes, map.partition, map.entities);
  validate(map);

  return map;
}

} // namespace aipb::map::importer
