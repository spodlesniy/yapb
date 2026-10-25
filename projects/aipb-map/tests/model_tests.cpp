#include <aipb/map/io/artifact.h>
#include <aipb/map/model.h>

#include "json.h"
#include "sha256.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void check(bool ok, const char *message) {
  if (!ok) {
    throw std::runtime_error(message);
  }
}

template <typename F> void rejected(F test, const char *message) {
  try {
    test();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error(message);
}

class TestDirectory {
public:
  std::filesystem::path path;
  TestDirectory() {
    path = std::filesystem::temp_directory_path() / "aipb_map_d203_artifact_tests";
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
  }
  ~TestDirectory() {
    std::filesystem::remove_all(path);
  }
};

aipb::map::MapModel exampleMap() {
  using namespace aipb::map;
  MapModel map;
  map.name = "cs_тест_\"\\";
  map.sourceFormat = "goldsrc-bsp";
  map.sourceSha256 = std::string(64, 'a');
  map.units = "goldsrc_units";
  map.meshes.push_back({ 9007199254740997ULL, { { 1.5, -2.25, 1000.125 }, { 7, 8, 9 }, { 12, 13, 14 } }, { { 0, 1, 2 } } });
  map.collision.push_back({ 2, "solid", { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } }, { { 0, 1, 2 } } });
  map.entities.push_back(
      { 3, "func_door", { 20.5, 33, -3 }, { { "key\n\"", "value \\ test" }, { "special", "\t\r\n" } }, map.meshes[0].id, 2 });

  return map;
}

void testJsonAndDigest() {
  using aipb::map::io::parseJson;
  using aipb::map::io::sha256Hex;
  using aipb::map::io::writeJson;
  check(sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256 empty vector incorrect");
  check(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 abc vector incorrect");
  check(parseJson(R"({"x":"\uD83D\uDE00", "a":[true,false,null,-2.25e1]})").at("x").text() == "😀", "escaped surrogate pair not parsed");
  check(writeJson(parseJson(R"({"b":1,"a":2})")) == R"({"a":2,"b":1})", "JSON object ordering not canonical");
  rejected([] { parseJson(R"({"x":1,"x":2})"); }, "duplicate JSON object keys accepted");
  rejected([] { parseJson(R"({"x":NaN})"); }, "invalid numeric token accepted");
  rejected([] { parseJson(R"({"x":1,})"); }, "trailing comma accepted");
  rejected([] { parseJson(R"({"x":"\uD800"})"); }, "unpaired surrogate accepted");
  rejected([] { parseJson(R"({"x":1e999})"); }, "nonfinite floating point accepted");
  rejected([] { parseJson(std::string("\"\xFF\"", 3)); }, "invalid raw UTF8 accepted");
}

void testStructuralValidation() {
  using namespace aipb::map;
  auto map = exampleMap();
  validate(map);
  auto broken = map;
  broken.meshes.push_back(broken.meshes.front());
  rejected([&] { validate(broken); }, "duplicate mesh ID accepted");
  broken = map;
  broken.meshes[0].triangles[0].c = 200;
  rejected([&] { validate(broken); }, "bad triangle accepted");
  broken = map;
  broken.entities[0].collisionId = 999;
  rejected([&] { validate(broken); }, "dangling collision accepted");
  broken = map;
  broken.entities[0].origin.x = std::numeric_limits<double>::infinity();
  rejected([&] { validate(broken); }, "infinite coordinate accepted");
  broken = map;
  broken.sourceSha256 = "0000";
  rejected([&] { validate(broken); }, "bad source hash accepted");

  NavigationModel nav;
  nav.mapArtifactSha256 = std::string(64, 'b');
  nav.regions.push_back({ 4, { map.meshes[0].id } });
  nav.nodes.push_back({ 1, { 0, 0, 0 }, 4, "junction" });
  nav.nodes.push_back({ 2, { 10, 0, 0 }, 4, "exit" });
  nav.edges.push_back({ 3, 1, 2, "walk", { { "width", 64.25 } } });
  validate(nav, &map);
  nav.edges[0].to = 404;
  rejected([&] { validate(nav); }, "dangling edge accepted");
  nav.edges[0].to = 2;
  nav.edges[0].parameters["width"] = std::nan("");
  rejected([&] { validate(nav); }, "nan traversal parameter accepted");
  nav.edges[0].parameters["width"] = 64.25;
  nav.regions[0].meshIds.push_back(12);
  rejected([&] { validate(nav, &map); }, "dangling map geometry accepted");
}

void testPersistence() {
  using namespace aipb::map;
  TestDirectory dir;
  const auto model = exampleMap();
  const auto mapPath = dir.path / "source.aipbmap";
  const auto digest = io::saveMap(model, mapPath);
  const auto loaded = io::loadMap(mapPath);
  check(digest == loaded.sha256, "map fingerprint is not stable");
  check(loaded.model == model, "map JSONL round-trip lost information");
  check(digest.size() == 64, "map fingerprint is not SHA-256");
  check(io::saveMap(model, dir.path / "identical.aipbmap") == digest, "identical map has different fingerprint");
  auto changed = model;
  changed.meshes[0].vertices[0].z += 1;
  check(io::saveMap(changed, dir.path / "changed.aipbmap") != digest, "changed geometry retained the old map fingerprint");
  rejected([&] { io::saveMap(model, mapPath); }, "overwrite of existing artifact accepted");

  NavigationModel nav;
  nav.mapArtifactSha256 = digest;
  nav.regions.push_back({ 4, { model.meshes[0].id } });
  nav.nodes.push_back({ 1, { 0, 0, -5 }, 4, "junction" });
  nav.nodes.push_back({ 2, { 10, 0, -8 }, 4, "exit" });
  nav.edges.push_back({ 3, 1, 2, "drop", { { "delta_z", -3.0 } } });
  const auto navPath = dir.path / "result.aipbnav";
  const auto navDigest = io::saveNavigation(nav, navPath);
  const auto reloaded = io::loadNavigation(navPath, &loaded);
  check(nav == reloaded.model && navDigest == reloaded.sha256, "navigation JSONL round-trip changed data");
  rejected(
      [&] {
        auto incorrect = loaded;
        incorrect.sha256 = std::string(64, '0');
        io::loadNavigation(navPath, &incorrect);
      },
      "mismatching map reference accepted");

  const auto path = mapPath / "entities.jsonl";
  {
    std::ofstream out(path, std::ios::app);
    out << "{}\n";
  }
  rejected([&] { io::loadMap(mapPath); }, "modified artifact stream accepted");
}

void testLegacyMapCompatibility() {
  using namespace aipb::map;
  TestDirectory dir;
  const auto path = dir.path / "legacy.aipbmap";
  std::filesystem::create_directories(path);
  io::Json::Object hashes;

  for (const char *file : { "meshes.jsonl", "collision.jsonl", "entities.jsonl" }) {
    std::ofstream(path / file, std::ios::binary).close();
    hashes.emplace(file, io::sha256Hex(""));
  }
  io::Json::Object manifest { { "format", "aipb-map" },
                              { "schema_version", "0.1.0" },
                              { "name", "legacy" },
                              { "source_format", "synthetic" },
                              { "source_sha256", std::string(64, 'a') },
                              { "units", "goldsrc_units" },
                              { "file_sha256", std::move(hashes) } };
  std::ofstream(path / "manifest.json") << io::writeJson(manifest) << '\n';
  const auto legacy = io::loadMap(path);
  check(legacy.model.name == "legacy" && legacy.model.meshes.empty() && !legacy.model.partition,
        "D203 v0.1 map artifact failed backward read");
  check(legacy.sha256.size() == 64, "legacy artifact hash unavailable");
}

void testEmptyAndVersioning() {
  using namespace aipb::map;
  TestDirectory dir;
  MapModel empty { "empty", "synthetic", std::string(64, '0'), "units", {}, {}, {} };
  auto digest = io::saveMap(empty, dir.path / "empty.aipbmap");
  check(io::loadMap(dir.path / "empty.aipbmap").model == empty, "empty map does not load");
  NavigationModel nav { digest, {}, {}, {} };
  auto navPath = dir.path / "empty.aipbnav";
  io::saveNavigation(nav, navPath);
  check(io::loadNavigation(navPath).model == nav, "empty graph does not load");
  const auto manifest = navPath / "manifest.json";
  {
    std::ifstream in(manifest);
    const auto contents = std::string(std::istreambuf_iterator<char>(in), {});
    const auto pos = contents.find("0.1.0");
    check(pos != std::string::npos, "schema version missing");
    auto changed = contents;
    changed.replace(pos, 5, "9.9.9");
    std::ofstream out(manifest, std::ios::trunc);
    out << changed;
  }
  rejected([&] { io::loadNavigation(navPath); }, "future schema accepted");
}

} // namespace

int main() {
  try {
    testJsonAndDigest();
    testStructuralValidation();
    testPersistence();
    testEmptyAndVersioning();
    testLegacyMapCompatibility();
    std::cout << "D203 model/artifact checks passed\n";

    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';

    return 1;
  }
}
