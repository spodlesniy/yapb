#pragma once

namespace aipb::explorer {

// One tick of local-space input. Axes are normalized, not world velocities.
struct MovementCommand {
  float forward {};  // [-1, 1], positive means forward.
  float strafe {};   // [-1, 1], positive means right.
  float yawDelta {}; // Degrees per tick in [-180, 180].
  bool jump {};
  bool duck {};
};

[[nodiscard]] bool isValid(const MovementCommand &command) noexcept;

}
