#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <aipb/map/importer/bsp_importer.h>
#include <aipb/map/io/artifact.h>

namespace {

void check(
  bool value,
  const char *reason
) {
  if (!value) {
    throw std::runtime_error(reason);
  }
}

void reject(
  std::invocable auto func,
  const char *reason
) {
  try {
    func();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error(reason);
}

struct TempDir {
  std::filesystem::path root = std::filesystem::temp_directory_path() / "aipb_map_d204_bsp_tests";
  TempDir() {
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
  }
  ~TempDir() {
    std::filesystem::remove_all(root);
  }
};

using Bytes = std::vector<std::uint8_t>;

void u16(
  Bytes &target,
  std::size_t offset,
  std::uint16_t value
) {
  target[offset] = static_cast<std::uint8_t>(value);
  target[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}
void u32(
  Bytes &target,
  std::size_t offset,
  std::uint32_t value
) {
  for (int i = 0; i < 4; ++i) {
    target[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
  }
}
void appendU16(
  Bytes &b,
  std::uint16_t v
) {
  const auto p = b.size();
  b.resize(p + 2);
  u16(b, p, v);
}
void appendU32(
  Bytes &b,
  std::uint32_t v
) {
  const auto p = b.size();
  b.resize(p + 4);
  u32(b, p, v);
}
void appendF32(
  Bytes &b,
  float v
) {
  appendU32(b, std::bit_cast<std::uint32_t>(v));
}

Bytes makeBsp(std::string_view entities = "{\n\"classname\" \"worldspawn\"\n\"wad\" \"c:\\valve\\test.wad\"\n}\n"
                                          "{\n\"classname\" \"func_door\"\n\"model\" \"*1\"\n\"origin\" \"2 3 4\"\n}\n",
              bool withCollision = false) {
  std::array<Bytes, 15> lumps;
  lumps[0].assign(entities.begin(), entities.end());
  lumps[0].push_back(0);

  for (const std::array<float, 3> v :
       { std::array<float, 3> { 0, 0, 0 }, { 64, 0, 0 }, { 64, 64, 0 }, { 0, 64, 0 }, { 0, 0, 32 }, { 64, 0, 32 }, { 0, 64, 32 } }) {
    for (float c : v) {
      appendF32(lumps[3], c);
    }
  }

  for (const std::array<std::uint16_t, 2> edge :
       { std::array<std::uint16_t, 2> { 3, 0 }, { 0, 1 }, { 1, 2 }, { 2, 3 }, { 5, 4 }, { 5, 6 }, { 6, 4 } }) {
    appendU16(lumps[12], edge[0]);
    appendU16(lumps[12], edge[1]);
  }

  for (auto e : { 1, 2, 3, 0, -4, 5, 6 }) {
    appendU32(lumps[13], std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(e)));
  }

  for (const std::array<int, 2> face : { std::array<int, 2> { 0, 4 }, { 4, 3 } }) {
    auto &b = lumps[7];
    appendU16(b, 0); // planenum
    appendU16(b, 0); // side
    appendU32(b, static_cast<std::uint32_t>(face[0]));
    appendU16(b, static_cast<std::uint16_t>(face[1]));
    appendU16(b, 0);           // texinfo
    appendU32(b, 0);           // light styles
    appendU32(b, 0xFFFFFFFFU); // light offset
  }

  if (withCollision) {
    appendF32(lumps[1], 0);
    appendF32(lumps[1], 0);
    appendF32(lumps[1], 1);
    appendF32(lumps[1], 32);
    appendU32(lumps[1], 2); // axial Z plane
    appendU32(lumps[5], 0); // hull-0 node, plane 0
    appendU16(lumps[5], 0xffff);
    appendU16(lumps[5], 0xfffe); // leaf 0 and leaf 1

    for (int n = 0; n < 6; ++n) {
      appendU16(lumps[5], 0); // bounds
    }
    appendU16(lumps[5], 0);
    appendU16(lumps[5], 0); // faces
    appendU32(lumps[9], 0); // clipnode plane 0
    appendU16(lumps[9], 0xfffe);
    appendU16(lumps[9], 0xffff); // solid, empty

    for (int contents : { -2, -1 }) {
      appendU32(lumps[10], static_cast<std::uint32_t>(contents));

      for (int n = 0; n < 24; ++n) {
        lumps[10].push_back(0);
      }
    }
  }

  for (int i = 0; i < 2; ++i) {
    auto &b = lumps[14];

    for (int n = 0; n < 3; ++n) {
      appendF32(b, withCollision ? -64.0f : 0.0f);
    }

    for (int n = 0; n < 3; ++n) {
      appendF32(b, withCollision ? 64.0f : 0.0f);
    }

    for (int n = 0; n < 3; ++n) {
      appendF32(b, 0);
    }

    for (int n = 0; n < 4; ++n) {
      appendU32(b, 0);
    }
    appendU32(b, 0); // visleafs
    appendU32(b, static_cast<std::uint32_t>(i));
    appendU32(b, 1);
  }
  Bytes result(124, 0);
  u32(result, 0, 30);

  for (std::size_t i = 0; i < lumps.size(); ++i) {
    u32(result, 4 + i * 8, static_cast<std::uint32_t>(result.size()));
    u32(result, 8 + i * 8, static_cast<std::uint32_t>(lumps[i].size()));
    result.insert(result.end(), lumps[i].begin(), lumps[i].end());
  }

  return result;
}

void write(
  const std::filesystem::path &path,
  const Bytes &bytes
) {
  std::ofstream file(path, std::ios::binary);
  file.write(
    reinterpret_cast<const char *>(bytes.data()),
    static_cast<std::streamsize>(bytes.size())
  );

  if (!file) {
    throw std::runtime_error("test BSP write failed");
  }
}

void testValidBsp(TempDir &dir) {
  const auto bspPath = dir.root / "synthetic.bsp";
  const auto source = makeBsp();
  write(bspPath, source);
  aipb::map::importer::BspImporter importer;
  const auto map = importer.importFile(bspPath);
  check(map.name == "synthetic", "map name");
  check(map.sourceFormat == "goldsrc-bsp-v30" && map.sourceSha256.size() == 64, "BSP metadata/hash");
  check(map.meshes.size() == 2 && map.collision.empty(), "world and brush meshes without guessed collision");
  check(map.meshes[0].id == 1 && map.meshes[0].triangles.size() == 2, "quad triangulation");
  check(map.meshes[1].id == 2 && map.meshes[1].triangles.size() == 1, "brush face triangulation");
  check(map.meshes[1].vertices[0] == aipb::map::Vec3 { 0, 0, 32 }, "negative surfedge orientation");
  check(map.entities.size() == 2, "entity count");
  check(map.entities[0].properties.at("wad") == "c:\\valve\\test.wad", "WAD path preserved");
  check(map.entities[0].meshId == 1 && map.entities[1].meshId == 2, "world/brush mesh references");
  check(map.entities[1].origin == aipb::map::Vec3 { 2, 3, 4 }, "entity origin");
  const auto directory = dir.root / "synthetic.aipbmap";
  const auto hash = aipb::map::io::saveMap(map, directory);
  const auto loaded = aipb::map::io::loadMap(directory);
  check(hash == loaded.sha256 && loaded.model == map, "BSP artifact round-trip");
  const auto secondPath = dir.root / "same.bsp";
  write(secondPath, source);
  const auto second = importer.importFile(secondPath);
  check(map.sourceSha256 == second.sourceSha256, "BSP source fingerprint not content-based");
}

std::size_t lumpOffset(
  const Bytes &bsp,
  std::size_t index
) {
  return static_cast<std::size_t>(bsp[4 + index * 8]) | (static_cast<std::size_t>(bsp[5 + index * 8]) << 8) |
         (static_cast<std::size_t>(bsp[6 + index * 8]) << 16) | (static_cast<std::size_t>(bsp[7 + index * 8]) << 24);
}

void testCollisionBsp(TempDir &dir) {
  using namespace aipb::map;
  importer::BspImporter importer;
  const auto path = dir.root / "collision.bsp";
  const auto source = makeBsp("{\"classname\" \"worldspawn\"} {\"classname\" \"func_door\" \"model\" \"*1\"}", true);
  write(path, source);
  const auto map = importer.importFile(path);
  check(map.partition.has_value(), "missing partition after importing collision BSP");
  const auto &p = *map.partition;
  check(p.planes.size() == 1 && p.planes[0].normal == Vec3 { 0, 0, 1 } && p.planes[0].distance == 32 && p.planes[0].type == 2,
        "plane values not imported");
  check(p.renderNodes.size() == 1 && p.renderNodes[0].front == -1 && p.renderNodes[0].back == -2, "hull 0 node/leaf indexing corrupted");
  check(p.clipNodes.size() == 1 && p.clipNodes[0].front == -2 && p.clipNodes[0].back == -1, "clipnode contents indexing corrupted");
  check(p.leaves.size() == 2 && p.leaves[0].contents == -2 && p.leaves[1].contents == -1, "leaf contents lost");
  check(p.models.size() == 2 && p.models[1].headNodes == std::array<std::int32_t, 4> { 0, 0, 0, 0 }, "model hull root mapping incorrect");
  check(p.models[0].mins.x == -64 && p.models[0].maxs.x == 64, "collision bounds lost");
  check(map.entities[0].partitionModelId == 1 && map.entities[1].partitionModelId == 2, "entity collision model links missing");
  const auto artifact = dir.root / "collision.aipbmap";
  const auto hash = io::saveMap(map, artifact);
  const auto decoded = io::loadMap(artifact);
  check(hash == decoded.sha256 && decoded.model == map, "collision artifact round-trip failed");
  std::ifstream manifest(artifact / "manifest.json");
  std::string contents((std::istreambuf_iterator<char>(manifest)), {});
  check(contents.find("0.3.0") != std::string::npos, "collision map schema not updated");

  auto invalid = map;
  invalid.partition->renderNodes[0].back = -3;
  reject([&] { validate(invalid); }, "missing hull-0 leaf accepted");
  invalid = map;
  invalid.partition->clipNodes[0].front = 3;
  reject([&] { validate(invalid); }, "missing clipnode accepted");
  invalid = map;
  invalid.partition->clipNodes[0].front = 0;
  reject([&] { validate(invalid); }, "clipnode cycle accepted");
  invalid = map;
  invalid.partition->planes[0].distance = std::numeric_limits<double>::infinity();
  reject([&] { validate(invalid); }, "non-finite collision plane accepted");
  invalid = map;
  invalid.entities[1].partitionModelId = 99;
  reject([&] { validate(invalid); }, "dangling collision-model entity accepted");

  auto bad = source;
  u32(bad, lumpOffset(bad, 9) + 0, 100); // invalid clipnode plane
  write(path, bad);
  reject([&] { importer.importFile(path); }, "missing clipnode plane accepted");
  bad = source;
  u16(bad, lumpOffset(bad, 5) + 4, 0); // self-cycle in render tree
  write(path, bad);
  reject([&] { importer.importFile(path); }, "BSP render-node cycle accepted");
  bad = source;
  u16(bad, lumpOffset(bad, 9) + 4, 0); // self-cycle in clip tree
  write(path, bad);
  reject([&] { importer.importFile(path); }, "BSP clipnode cycle accepted");
  bad = source;
  u32(bad, 8 + 9 * 8, 7); // incomplete clipnode record
  write(path, bad);
  reject([&] { importer.importFile(path); }, "partial clipnode accepted");
  bad = source;
  u32(bad, lumpOffset(bad, 14) + 36 + 4, 100); // invalid hull 1 root
  write(path, bad);
  reject([&] { importer.importFile(path); }, "invalid hull root accepted");
  bad = source;
  u32(bad, lumpOffset(bad, 10), 0); // invalid leaf contents
  write(path, bad);
  reject([&] { importer.importFile(path); }, "invalid BSP leaf contents accepted");
  bad = source;
  u32(bad, lumpOffset(bad, 1) + 16, 6); // unknown plane type
  write(path, bad);
  reject([&] { importer.importFile(path); }, "invalid BSP plane type accepted");
  bad = source;
  u32(bad, lumpOffset(bad, 1) + 12, 0x7f800000); // infinite plane distance
  write(path, bad);
  reject([&] { importer.importFile(path); }, "infinite BSP collision plane accepted");
  std::ofstream tamper(artifact / "partition_clipnodes.jsonl", std::ios::app);
  tamper << "{}\n";
  tamper.close();
  reject([&] { io::loadMap(artifact); }, "tampered partition stream accepted");
}

void testCorruptBsp(TempDir &dir) {
  aipb::map::importer::BspImporter importer;
  const auto path = dir.root / "invalid.bsp";
  auto source = makeBsp();
  auto bad = source;
  u32(bad, 0, 29);
  write(path, bad);
  reject([&] { importer.importFile(path); }, "BSP29 accepted");
  write(path, Bytes(32, 0));
  reject([&] { importer.importFile(path); }, "truncated header accepted");
  bad = source;
  u32(bad, 4 + 7 * 8, static_cast<std::uint32_t>(source.size() + 1));
  write(path, bad);
  reject([&] { importer.importFile(path); }, "out-of-range lump accepted");
  bad = source;
  u32(bad, 8 + 12 * 8, 5);
  write(path, bad);
  reject([&] { importer.importFile(path); }, "partial edge accepted");
  bad = source;
  const auto faceOffset = static_cast<std::size_t>(bad[4 + 7 * 8]) | (static_cast<std::size_t>(bad[5 + 7 * 8]) << 8);
  u32(bad, faceOffset + 4, 0x7FFFFFFF);
  write(path, bad);
  reject([&] { importer.importFile(path); }, "invalid face surfedge accepted");
  bad = source;
  const auto surfOffset = static_cast<std::size_t>(bad[4 + 13 * 8]) | (static_cast<std::size_t>(bad[5 + 13 * 8]) << 8);
  u32(bad, surfOffset, 999999);
  write(path, bad);
  reject([&] { importer.importFile(path); }, "invalid edge index accepted");
  const auto duplicateKey = makeBsp(R"({"classname" "light" "classname" "worldspawn" "message" "first" "message" "last"})");
  write(path, duplicateKey);
  const auto repeated = importer.importFile(path);
  check(repeated.entities[0].sourceProperties.size() == 4, "duplicate BSP entity properties not preserved");
  check(repeated.entities[0].sourceProperties[2].second == "first" && repeated.entities[0].sourceProperties[3].second == "last",
        "ordered duplicate BSP property values lost");
  check(repeated.entities[0].properties.at("message") == "last", "effective BSP property must use last value");
  check(repeated.entities[0].properties.at("classname") == "worldspawn", "effective entity classname incorrect");
  const auto repeatedArtifact = dir.root / "repeated.aipbmap";
  io::saveMap(repeated, repeatedArtifact);
  check(io::loadMap(repeatedArtifact).model == repeated, "ordered duplicate properties lost on artifact round-trip");
  const auto invalidModel = makeBsp("{\"classname\" \"worldspawn\"} {\"classname\" \"func_door\" \"model\" \"*99\"}");
  write(path, invalidModel);
  reject([&] { importer.importFile(path); }, "invalid brush model accepted");
  const auto badOrigin = makeBsp("{\"classname\" \"worldspawn\" \"origin\" \"1 2 bad\"}");
  write(path, badOrigin);
  reject([&] { importer.importFile(path); }, "invalid origin accepted");
  const auto missingWorld = makeBsp("{\"classname\" \"light\"}");
  write(path, missingWorld);
  reject([&] { importer.importFile(path); }, "missing worldspawn accepted");
}

} // namespace

int main(
  int argc,
  char *argv[]
) {
  try {
    if (argc == 3 && std::string_view { argv[1] } == "--write-cli-fixture") {
      write(argv[2], makeBsp());

      return 0;
    }

    if (argc != 1) {
      throw std::invalid_argument("unsupported BSP test invocation");
    }

    TempDir dir;
    testValidBsp(dir);
    testCollisionBsp(dir);
    testCorruptBsp(dir);
    std::cout << "D205 BSP collision import checks passed\n";

    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';

    return 1;
  }
}
