#pragma once

#include <aipb/map/model.h>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace aipb::map::collision {

inline constexpr std::int32_t kContentsEmpty = -1;
inline constexpr std::int32_t kContentsSolid = -2;

enum class PlayerStance { Standing, Crouching };

struct TraceResult {
  double fraction { 1.0 };
  Vec3 endPosition {};
  bool startSolid {};
  bool allSolid {};
  bool inOpen {};
  bool inLiquid {};
  std::optional<CollisionPlane> impactPlane {};
};

// Read-only queries against validated static BSP partitions in model-local coordinates.
// The referenced map must outlive this non-owning query object.
class CollisionQueries {
public:
  explicit CollisionQueries(const MapModel &map);

  std::int32_t pointContents(
    ObjectId modelId,
    std::size_t hull,
    Vec3 point
  ) const;

  TraceResult traceSegment(
    ObjectId modelId,
    std::size_t hull,
    Vec3 start,
    Vec3 end
  ) const;

  bool canOccupy(
    ObjectId modelId,
    PlayerStance stance,
    Vec3 origin
  ) const;

  bool canTraverse(
    ObjectId modelId,
    PlayerStance stance,
    Vec3 start,
    Vec3 end
  ) const;

private:
  const CollisionPartition &partition_;

  std::int32_t root(
    ObjectId modelId,
    std::size_t hull
  ) const;

  std::int32_t contents(
    std::size_t hull,
    std::int32_t terminal
  ) const;
};

} // namespace aipb::map::collision
