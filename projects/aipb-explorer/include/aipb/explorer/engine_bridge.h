#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// POD-only boundary shared by standalone C++20 and GoldSrc C++17 code.
typedef struct AipbExplorerBridgeCommand {
  float forward;
  float strafe;
  float yaw_delta;
  uint8_t jump;
  uint8_t duck;
} AipbExplorerBridgeCommand;

typedef struct AipbExplorerBridgeState {
  double x, y, z;
  double vx, vy, vz;
  float yaw, pitch;
  uint64_t tick;
  uint8_t on_ground;
  uint8_t crouching;
} AipbExplorerBridgeState;

typedef struct AipbExplorerBridgeStep {
  AipbExplorerBridgeState state;
  uint8_t status;
  uint8_t collided;
  uint8_t started_solid;
} AipbExplorerBridgeStep;

typedef struct AipbExplorerBridgeApi {
  void *context;
  // Optional: real engines may prohibit restoring an arbitrary state.
  int (*reset)(void *, const AipbExplorerBridgeState *, AipbExplorerBridgeState *);
  int (*step)(void *, const AipbExplorerBridgeCommand *, AipbExplorerBridgeStep *);
  double tick_seconds;
} AipbExplorerBridgeApi;

#ifdef __cplusplus
}
#endif
