#include <aipb/map/collision/queries.h>
#include <aipb/map/io/artifact.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace aipb::map;
using namespace aipb::map::collision;

void check(
  bool value,
  const char *message
) {
  if (!value) {
    throw std::runtime_error(message);
  }
}

template <class F> void reject(
  F function,
  const char *message
) {
  try {
    function();
  } catch (const std::invalid_argument &) {
    return;
  }

  throw std::runtime_error(message);
}

MapModel fixture() {
  MapModel map;
  map.name = "collision_queries";
  map.sourceFormat = "synthetic";
  map.sourceSha256 = std::string(64, 'a');
  map.units = "goldsrc_units";
  map.partition = CollisionPartition {
    .planes = {
      { { 1, 0, 0 }, 0, 0 },
      { { 0, 1, 0 }, 0, 1 },
      { { 0, 0, 1 }, 0, 2 },
      { { 1, 1, 0 }, 0, 3 }
    },
    .renderNodes = { { 0, -2, 1 }, { 1, -1, -3 } },
    .clipNodes = { { 0, -1, 1 }, { 1, -2, -3 }, { 2, -2, -1 }, { 3, -1, -2 } },
    .leaves = { { kContentsSolid }, { kContentsEmpty }, { -3 } },
    .models = {
      { 1, { -64, -64, -64 }, { 64, 64, 64 }, {}, { 0, 0, 3, 2 } },
      { 2, { -64, -64, -64 }, { 64, 64, 64 }, {}, { -1, -2, -1, -2 } }
    }
  };
  validate(map);

  return map;
}

void testQueries() {
  const auto map = fixture();
  CollisionQueries queries(map);
  check(queries.pointContents(1, 0, { 5, 5, 0 }) == kContentsEmpty, "hull 0 empty");
  check(queries.pointContents(1, 0, { -5, 5, 0 }) == kContentsSolid, "hull 0 solid");
  check(queries.pointContents(1, 0, { -5, -5, 0 }) == -3, "hull 0 water");
  check(queries.pointContents(1, 0, { 0, 5, 0 }) == kContentsEmpty, "boundary front");
  check(queries.pointContents(1, 1, { -5, -5, 0 }) == -3, "clip contents");
  check(queries.pointContents(2, 0, {}) == kContentsSolid, "leaf terminal root");
  check(queries.pointContents(2, 2, {}) == kContentsEmpty, "clip terminal root");
  check(queries.pointContents(1, 2, { 2, 2, 0 }) == kContentsEmpty, "nonaxial open");
  check(queries.pointContents(1, 2, { -2, -2, 0 }) == kContentsSolid, "nonaxial solid");
  check(queries.canOccupy(1, PlayerStance::Standing, { 5, 2, 0 }), "standing free");
  check(!queries.canOccupy(1, PlayerStance::Standing, { -5, 2, 0 }), "standing solid");
  check(!queries.canOccupy(1, PlayerStance::Crouching, { 5, 2, 1 }), "crouch solid");
  check(queries.canOccupy(1, PlayerStance::Crouching, { 5, 2, -1 }), "crouch free");
  const auto hit = queries.traceSegment(1, 1, { 5, 2, 0 }, { -5, 2, 0 });
  check(hit.fraction == 0.5 && hit.endPosition == Vec3 { 0, 2, 0 }, "fraction and endpoint");
  check(hit.impactPlane && hit.impactPlane->normal == Vec3 { 1, 0, 0 }, "impact plane");
  check(!hit.startSolid && !hit.allSolid && hit.inOpen, "trace flags");
  check(!queries.canTraverse(1, PlayerStance::Standing, { 5, 2, 0 }, { -5, 2, 0 }), "blocked movement");
  check(queries.canTraverse(1, PlayerStance::Standing, { 5, 2, 0 }, { 6, 2, 0 }), "clear movement");
  const auto reverse = queries.traceSegment(1, 1, { -5, 2, 0 }, { 5, 2, 0 });
  check(reverse.startSolid && !reverse.allSolid && reverse.fraction == 0, "start solid");
  const auto trapped = queries.traceSegment(1, 1, { -5, 2, 0 }, { -4, 2, 0 });
  check(trapped.startSolid && trapped.allSolid, "all solid");
  const auto water = queries.traceSegment(1, 0, { 5, -2, 0 }, { -5, -2, 0 });
  check(water.inLiquid && water.inOpen && !water.impactPlane, "water not solid");
  const auto diagonal = queries.traceSegment(1, 2, { 2, 2, 0 }, { -2, -2, 0 });
  check(diagonal.fraction == 0.5 && diagonal.impactPlane, "nonaxial trace");
  const auto upward = queries.traceSegment(1, 3, { 5, 1, -2 }, { 5, 1, 2 });
  check(upward.fraction == 0.5 && upward.impactPlane &&
        upward.impactPlane->normal == Vec3 { 0, 0, -1 }, "reversed impact normal");
  check(queries.traceSegment(2, 1, {}, {}).allSolid, "terminal solid trace");
  check(!queries.traceSegment(2, 2, {}, {}).allSolid, "terminal empty trace");
  reject([&] { (void)queries.pointContents(99, 0, {}); }, "unknown model accepted");
  reject([&] { (void)queries.pointContents(1, 4, {}); }, "invalid hull accepted");
  reject([&] { (void)queries.pointContents(1, 1, { std::numeric_limits<double>::infinity(), 0, 0 }); },
         "nonfinite point accepted");
  auto absent = map;
  absent.partition.reset();
  reject([&] { (void)CollisionQueries(absent); }, "missing partition accepted");
}

void testDeepTreeAndArtifact() {
  auto map = fixture();
  auto &part = *map.partition;
  part.clipNodes.clear();
  constexpr std::size_t count = 4096;

  for (std::size_t i = 0; i < count; ++i) {
    part.clipNodes.push_back({ 0, i + 1 == count ? -1 : static_cast<std::int32_t>(i + 1), -2 });
  }
  part.models[0].headNodes = { 0, 0, -1, -1 };
  validate(map);
  const CollisionQueries queries(map);
  check(queries.pointContents(1, 1, { 1, 0, 0 }) == kContentsEmpty, "deep contents");
  check(queries.traceSegment(1, 1, { 1, 0, 0 }, { 2, 0, 0 }).fraction == 1.0, "deep trace");
  const auto dir = std::filesystem::temp_directory_path() / "aipb_map_d207_collision";
  std::filesystem::remove_all(dir);

  try {
    const auto hash = io::saveMap(map, dir);
    const auto decoded = io::loadMap(dir);
    check(decoded.sha256 == hash && decoded.model == map, "artifact roundtrip");
    const CollisionQueries restored(decoded.model);
    check(restored.pointContents(1, 1, { 1, 0, 0 }) == kContentsEmpty, "reloaded partition query");
  } catch (...) {
    std::filesystem::remove_all(dir);
    throw;
  }
  std::filesystem::remove_all(dir);
}

} // namespace

int main() {
  try {
    testQueries();
    testDeepTreeAndArtifact();
    std::cout << "D207 collision queries passed\n";

    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';

    return 1;
  }
}
