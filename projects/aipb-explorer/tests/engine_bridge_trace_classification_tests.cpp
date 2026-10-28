#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_trace_classification.h>

using namespace aipb::explorer::bridge;

static void require(bool condition) {
  if (!condition) {
    throw std::runtime_error("movement classification regression");
  }
}

static TraceBuffer buildGroundTrace(
  int frames,
  float forward,
  float strafe,
  double dx,
  double dy,
  float initialYaw = 0.0f
) {
  TraceBuffer trace {};
  AipbExplorerBridgeState previous {};
  previous.on_ground = 1;
  previous.yaw = initialYaw;

  for (int i = 0; i < frames; ++i) {
    TraceSample step {};
    step.command.forward = forward;
    step.command.strafe = strafe;
    step.maxSpeed = 250.0f;
    step.msec = 25;
    step.before = previous;
    step.after = step.before;
    step.after.tick = step.before.tick + 1;
    step.after.x += dx;
    step.after.y += dy;
    require(trace.append(step));
    previous = step.after;
  }

  trace.completed = true;
  return trace;
}

static int runTests() {
  TraceMovementClassification result {};
  TraceBuffer empty {};
  require(!classifyTraceMovement(empty, result));
  require(result.firstBlockedStep == -1);

  TraceBuffer stuck = buildGroundTrace(12, 1.0f, 0.0f, 0.0, 0.0);
  require(classifyTraceMovement(stuck, result));
  require(result.classification == MovementClassification::BlockedCandidate);
  require(result.evaluatedWindows == 3 && result.lowTravelWindows == 3);
  require(result.longestLowTravelStreak == 3 && result.firstBlockedStep == 0);
  require(result.eligibleFrames == 12 && result.requestedUnits == 75.0);
  require(result.observedUnits == 0.0 && result.projectedUnits == 0.0);
  require(std::strcmp(movementClassificationName(result.classification), "BlockedCandidate") == 0);

  TraceBuffer oneWindow = buildGroundTrace(4, 1.0f, 0.0f, 0.0, 0.0);
  require(classifyTraceMovement(oneWindow, result));
  require(result.classification == MovementClassification::InsufficientEvidence);
  require(result.lowTravelWindows == 1 && result.firstBlockedStep == -1);

  // A slow start followed by acceleration is not a persistent blockage.
  TraceBuffer accelerating = buildGroundTrace(8, 1.0f, 0.0f, 0.0, 0.0);

  for (int i = 4; i < 8; ++i) {
    accelerating.samples[i].before.x += (i - 4) * 5.0;
    accelerating.samples[i].after.x += (i - 3) * 5.0;
  }

  require(classifyTraceMovement(accelerating, result));
  require(result.classification == MovementClassification::Progress);
  require(result.lowTravelWindows == 1 && result.progressWindows == 1);
  require(result.firstBlockedStep == -1);

  TraceBuffer walking = buildGroundTrace(8, 1.0f, 0.0f, 5.0, 0.0);
  require(classifyTraceMovement(walking, result));
  require(result.classification == MovementClassification::Progress);
  require(result.progressWindows == 2 && result.projectedUnits == 40.0);

  TraceBuffer strafing = buildGroundTrace(8, 0.0f, 1.0f, 0.0, -5.0);
  require(classifyTraceMovement(strafing, result));
  require(result.classification == MovementClassification::Progress);
  require(result.projectedUnits == 40.0);

  TraceBuffer lateral = buildGroundTrace(8, 1.0f, 0.0f, 0.0, 5.0);
  require(classifyTraceMovement(lateral, result));
  require(result.classification == MovementClassification::InsufficientEvidence);
  require(result.directionMismatchWindows == 2 && result.lowTravelWindows == 0);

  TraceBuffer reversed = buildGroundTrace(8, 1.0f, 0.0f, -5.0, 0.0);
  require(classifyTraceMovement(reversed, result));
  require(result.classification == MovementClassification::InsufficientEvidence);
  require(result.projectedUnits == -40.0);

  TraceBuffer turned = buildGroundTrace(8, 1.0f, 0.0f, 0.0, 5.0, 90.0f);
  require(classifyTraceMovement(turned, result));
  require(result.classification == MovementClassification::Progress);
  require(std::abs(result.projectedUnits - 40.0) < 0.00001);

  // Neutral frames interrupt short windows rather than joining them.
  TraceBuffer interrupted = buildGroundTrace(7, 1.0f, 0.0f, 0.0, 0.0);
  interrupted.samples[3].command.forward = 0.0f;
  require(classifyTraceMovement(interrupted, result));
  require(result.classification == MovementClassification::InsufficientEvidence);
  require(result.evaluatedWindows == 0);

  TraceBuffer shortTrace = buildGroundTrace(2, 1.0f, 0.0f, 0.0, 0.0);
  require(classifyTraceMovement(shortTrace, result));
  require(result.classification == MovementClassification::InsufficientEvidence);

  // Invalid evidence must not overwrite a prior classification.
  result.firstBlockedStep = 123;
  walking.samples[4].after.x = std::numeric_limits<double>::quiet_NaN();
  require(!classifyTraceMovement(walking, result));
  require(result.firstBlockedStep == 123);

  return 0;
}

int main() {
  try {
    return runTests();
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "aipb_explorer_trace_classification: %s\n", error.what());
    return 1;
  }
}
