#pragma once

#include <aipb/explorer/engine_bridge.h>
#include <aipb/explorer/environment.h>

namespace aipb::explorer {

// Synchronous adapter for a controlled GoldSrc engine bridge.
// The runtime supplies the callbacks; the standalone core links no engine code.
class GoldSrcEnvironment final : public IExplorationEnvironment {
public:
  explicit GoldSrcEnvironment(AipbExplorerBridgeApi bridge);

  [[nodiscard]] double tickSeconds() const noexcept override;
  MovementState reset(const MovementState &initial) override;
  MovementResult step(const MovementCommand &command) override;

private:
  AipbExplorerBridgeApi bridge_ {};
  MovementState current_ {};
  bool initialized_ {};
  bool terminal_ {};
};

}
