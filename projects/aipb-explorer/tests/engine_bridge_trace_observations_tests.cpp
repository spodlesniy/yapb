#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_trace_observations.h>

using namespace aipb::explorer::bridge;

static void require(bool condition) {
  if (!condition) {
    throw std::runtime_error("observation regression");
  }
}

static TraceBuffer traceWithMovement(int moving, int lateral, int stopped) {
  TraceBuffer trace {};
  trace.botIndex = 1;
  AipbExplorerBridgeState prior {};
  prior.on_ground = 1;

  for (int i = 0; i < moving + lateral + stopped; ++i) {
    TraceSample sample {};
    sample.before = prior;
    sample.after = prior;
    ++sample.after.tick;
    sample.msec = 25;
    sample.maxSpeed = 250.0f;
    sample.command.forward = 1.0f;

    if (i < moving) {
      sample.after.x += 5.0;
    }
    else if (i < moving + lateral) {
      sample.after.y += 5.0;
    }

    require(trace.append(sample));
    prior = sample.after;
  }

  trace.completed = true;
  return trace;
}

static int runTests() {
  TraceBuffer trace = traceWithMovement(4, 12, 0);
  const char *path = "aipb_observations_test.csv";
  std::remove(path);
  ObservationWriteResult result {};
  require(writeTraceObservations(trace, "de_dust2", path, result));
  require(result.records == 3 && result.progress == 1);
  require(result.deflectedCandidates == 2 && result.blockedCandidates == 0);
  std::FILE *file = openTraceFile(path, "rt");
  require(file != nullptr);
  char line[1024] {};
  require(std::fgets(line, sizeof(line), file) != nullptr);
  require(std::strcmp(line, "aipb-explorer-observations-v1,de_dust2,1,16\n") == 0);
  require(std::fgets(line, sizeof(line), file) != nullptr);
  require(std::strcmp(line, kObservationCsvColumns) == 0);
  require(std::fgets(line, sizeof(line), file) != nullptr);
  require(std::strstr(line, ",Progress,unknown,") != nullptr);
  require(std::fgets(line, sizeof(line), file) != nullptr);
  require(std::strstr(line, ",DeflectedCandidate,unknown,") != nullptr);
  require(std::fclose(file) == 0);
  ObservationWriteResult unchanged = result;
  require(!writeTraceObservations(trace, "de_dust2", path, result));
  require(result.records == unchanged.records);
  std::remove(path);

  TraceBuffer stalled = traceWithMovement(0, 0, 12);
  require(writeTraceObservations(stalled, "de_dust2", path, result));
  require(result.blockedCandidates == 2 && result.records == 2);
  std::remove(path);

  TraceBuffer shortTrace = traceWithMovement(0, 0, 4);
  result.records = 19;
  require(!writeTraceObservations(shortTrace, "de_dust2", path, result));
  require(result.records == 19);
  require(!writeTraceObservations(stalled, "../bad", path, result));
  require(!writeTraceObservations(stalled, "de_dust2", "../", result));
  require(!writeTraceObservations(stalled, "de_dust2", nullptr, result));

  return 0;
}

int main() {
  try {
    return runTests();
  }
  catch (const std::exception &error) {
    std::fprintf(stderr, "aipb_explorer_trace_observations: %s\n", error.what());
    return 1;
  }
}
