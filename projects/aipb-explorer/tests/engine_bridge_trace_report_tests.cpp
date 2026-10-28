#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_trace_report.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("trace report regression");
  }
}

int main() {
  try {
    TraceBuffer trace {};
    TraceReport report {};
    require(!summarizeTrace(trace, report));
    trace.botIndex = 1;
    TraceSample first {};
    first.msec = 10;
    first.maxSpeed = 250.0f;
    first.command.forward = 1.0f;
    first.before.on_ground = 1;
    first.after = first.before;
    first.after.tick = 1;
    first.after.x = 3.0;
    first.after.y = 4.0;
    first.after.vx = 300.0;
    require(trace.append(first));
    require(!summarizeTrace(trace, report));
    TraceSample second {};
    second.msec = 20;
    second.maxSpeed = 250.0f;
    second.before = first.after;
    second.after = second.before;
    second.after.tick = 2;
    second.after.x = 3.0;
    second.after.y = 8.0;
    second.after.on_ground = 0;
    second.after.crouching = 1;
    second.after.vx = 0.0;
    second.after.vy = 200.0;
    require(trace.append(second));
    trace.completed = true;
    require(summarizeTrace(trace, report));
    require(report.frames == 2 && report.simulatedMs == 30);
    require(report.traveledUnits == 9.0);
    require(report.displacementUnits == std::sqrt(73.0));
    require(std::abs(report.averageSpeed - 300.0) < 0.00001);
    require(report.peakVelocity == 300.0);
    require(report.airborneFrames == 1 && report.crouchedFrames == 1);
    require(report.jumpHeldFrames == 0 && report.neutralInputFrames == 1);
    report.frames = -1;
    trace.samples[1].before.x += 2.0;
    require(!summarizeTrace(trace, report) && report.frames == -1);
    trace.clear();
    require(!summarizeTrace(trace, report));
    // Two full windows: one stationary input and one successful movement.
    TraceBuffer progressTrace {};
    AipbExplorerBridgeState previous {};
    previous.on_ground = 1;

    for (int i = 0; i < 8; ++i) {
      TraceSample step {};
      step.msec = 25;
      step.maxSpeed = 250.0f;
      step.command.forward = 1.0f;
      step.before = previous;
      step.after = step.before;
      ++step.after.tick;

      if (i >= 4) {
        step.after.x += 5.0;
      }

      require(progressTrace.append(step));
      previous = step.after;
    }

    TraceProgressReport progress {};
    require(!summarizeTraceProgress(progressTrace, progress));
    progressTrace.completed = true;
    require(summarizeTraceProgress(progressTrace, progress));
    require(progress.groundedInputFrames == 8 && progress.evaluatedWindows == 2);
    require(progress.lowProgressWindows == 1 && progress.firstLowProgressStep == 0);
    require(progress.longestLowProgressMs == 100);
    require(progress.requestedUnits == 50.0 && progress.observedUnits == 20.0);

    // Invalid evidence never partially mutates the previous result.
    progressTrace.samples[6].after.x = std::numeric_limits<double>::quiet_NaN();
    progress.firstLowProgressStep = 77;
    require(!summarizeTraceProgress(progressTrace, progress));
    require(progress.firstLowProgressStep == 77);
    progressTrace.clear();

    // Neutral frames interrupt short windows rather than joining them.
    TraceBuffer interrupted {};
    previous = {};
    previous.on_ground = 1;

    for (int i = 0; i < 3; ++i) {
      TraceSample step {};
      step.msec = 90;
      step.maxSpeed = 250.0f;
      step.command.forward = i == 1 ? 0.0f : 1.0f;
      step.before = previous;
      step.after = step.before;
      ++step.after.tick;
      require(interrupted.append(step));
      previous = step.after;
    }

    interrupted.completed = true;
    progress = {};
    require(summarizeTraceProgress(interrupted, progress));
    require(progress.groundedInputFrames == 2 && progress.evaluatedWindows == 0);
    require(progress.lowProgressWindows == 0 && progress.firstLowProgressStep == -1);
    interrupted.clear();
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "aipb_explorer_trace_report: %s\n", e.what());
    return 1;
  }
}
