#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_trace_io.h>

using namespace aipb::explorer::bridge;

static void require(bool ok) {
  if (!ok) {
    throw std::runtime_error("named trace I/O contract failed");
  }
}

static int runTests() {
  require(safeName("jump_B_01"));
  require(safeName("test-2"));
  require(!safeName("../escape"));
  require(!safeName("white space"));
  require(!safeName(""));
  require(!safeName("dot.name"));
  require(!safeName("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"));
  char b[1024] {};
  TraceMetadata metadata {};
  std::snprintf(metadata.map, sizeof(metadata.map), "%s", "de_dust2");
  metadata.initial.x = 12.5;
  metadata.initial.y = -2.75;
  metadata.initial.z = -92.0;
  metadata.initial.vx = 21.0;
  metadata.initial.yaw = -90.0f;
  metadata.initial.on_ground = 1;
  metadata.viewZ = 17.0f;
  require(formatMetadata(b, sizeof(b), metadata));
  TraceMetadata restored {};
  require(parseMetadata(b, restored));
  require(restored.initial.x == metadata.initial.x);
  require(restored.initial.y == metadata.initial.y);
  require(restored.initial.yaw == metadata.initial.yaw);
  require(restored.viewZ == 17.0f);
  require(std::strcmp(restored.map, metadata.map) == 0);
  require(!parseMetadata("aipb-explorer-v1,de_dust2,1", restored));
  require(!parseMetadata("aipb-explorer-v2,../bad,1", restored));
  require(!formatMetadata(b, 8, metadata));

  TraceSample sample {};
  sample.command = { 1.0f, -0.5f, 3.0f, 1, 0 };
  sample.before = metadata.initial;
  sample.before.tick = 0;
  sample.after = metadata.initial;
  sample.after.tick = 1;
  sample.after.x = 13.25;
  sample.after.yaw = -87.0f;
  sample.maxSpeed = 250.0f;
  sample.oldButtons = 4;
  sample.msec = 2;
  require(formatSample(b, sizeof(b), 0, sample));
  TraceSample decoded {};
  require(parseSample(b, 0, decoded));
  require(decoded.command.forward == sample.command.forward);
  require(decoded.command.strafe == sample.command.strafe);
  require(decoded.before.x == sample.before.x);
  require(decoded.after.x == sample.after.x);
  require(decoded.after.yaw == sample.after.yaw);
  require(decoded.before.tick == 0 && decoded.after.tick == 1);
  require(decoded.oldButtons == 4);
  require(decoded.msec == 2);
  require(!parseSample(b, 1, decoded));
  require(!parseSample("0,2,0", 0, decoded));
  require(!formatSample(b, 5, 0, sample));
  sample.after.x = std::numeric_limits<double>::quiet_NaN();
  require(!formatSample(b, sizeof(b), 0, sample));

  return 0;
}

int main() {
  try {
    return runTests();
  } catch (const std::exception &error) {
    std::fprintf(stderr, "aipb_explorer_named_trace_io: %s\n", error.what());
    return 1;
  }
}
