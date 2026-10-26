#pragma once

#include <aipb/map/model.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <vector>

namespace aipb::map::navigation::detail {

inline constexpr std::uint32_t kOnlyTrigger = 1u;
inline constexpr std::uint32_t kTouch = 2u;
inline constexpr std::uint32_t kPressure = 4u;
inline constexpr std::uint32_t kInstantCrowbar = 256u;

enum class BrushRole {
  FixedSupport,
  TriggerSupport,
  MutableObstacle
};

struct Brush {
  const MapEntity *entity {};
  const CollisionPartitionModel *model {};
  BrushRole role { BrushRole::MutableObstacle };
};

inline bool suppliesSupport(BrushRole role) {
  return role != BrushRole::MutableObstacle;
}

inline BrushRole classifyBreakable(const MapEntity &entity) {
  const auto it = entity.properties.find("spawnflags");
  std::uint32_t flags = 0;

  if (it != entity.properties.end() && !it->second.empty()) {
    const auto &value = it->second;
    const auto [end, status] = std::from_chars(value.data(), value.data() + value.size(), flags);

    if (status != std::errc {} || end != value.data() + value.size()) {
      return BrushRole::MutableObstacle;
    }
  }

  // Only Trigger disables the GoldSrc touch handler and ordinary damage.
  // Treat Touch and Instant Crowbar combinations conservatively.
  if ((flags & kOnlyTrigger) != 0 && (flags & (kTouch | kInstantCrowbar)) == 0) {
    return BrushRole::TriggerSupport;
  }

  return BrushRole::MutableObstacle;
}

inline std::vector<Brush> collectBrushes(const MapModel &map) {
  std::vector<Brush> result;

  if (!map.partition || map.partition->models.empty()) {
    return result;
  }
  const auto &models = map.partition->models;
  const auto worldId = models.front().id;

  for (const auto &entity : map.entities) {
    if (!entity.partitionModelId ||
        (entity.type != "func_wall" && entity.type != "func_breakable")) {
      continue;
    }
    const auto it = std::find_if(models.begin(), models.end(), [&](const CollisionPartitionModel &model) {
      return model.id == *entity.partitionModelId;
    });

    if (it == models.end() || it->id == worldId) {
      continue;
    }
    const auto role = entity.type == "func_wall" ? BrushRole::FixedSupport : classifyBreakable(entity);
    result.push_back({
      .entity = &entity,
      .model = &*it,
      .role = role
    });
  }

  return result;
}

} // namespace aipb::map::navigation::detail
