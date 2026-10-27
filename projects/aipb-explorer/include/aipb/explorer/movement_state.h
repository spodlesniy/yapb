#pragma once

#include <cstdint>

#include <aipb/explorer/types.h>

namespace aipb::explorer {

enum class Stance {
  Standing,
  Crouching
};

// Snapshot at a tick boundary, before or after applying one command.
struct MovementState {
  Vec3 position {};
  Vec3 velocity {};
  float yaw {};   // Degrees in [-180, 180).
  float pitch {}; // Degrees in [-90, 90].
  Stance stance { Stance::Standing };
  bool onGround {};
  std::uint64_t tick {};

  bool operator==(const MovementState &) const = default;
};

[[nodiscard]] bool isValid(const MovementState &state) noexcept;

}
