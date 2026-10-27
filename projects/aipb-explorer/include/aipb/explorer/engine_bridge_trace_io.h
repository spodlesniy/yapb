#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include <aipb/explorer/engine_bridge_trace.h>

namespace aipb::explorer::bridge {

// V2 traces are map-bound. The first CSV record is the engine-observed start.
struct TraceMetadata {
  char map[64] {};
  AipbExplorerBridgeState initial {};
  float viewZ {};
};

inline bool safeName(const char *name, std::size_t maximum = 48) {
  if (!name || !*name) {
    return false;
  }
  std::size_t size = 0;
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(name); *p; ++p) {
    if (++size > maximum || !( (*p >= 'a' && *p <= 'z')
      || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')
      || *p == '_' || *p == '-')) {
      return false;
    }
  }
  return true;
}

inline bool validMetadata(const TraceMetadata &meta) {
  return safeName(meta.map, 63) && validState(meta.initial)
    && std::isfinite(meta.viewZ) && meta.viewZ > -128.0f && meta.viewZ < 128.0f;
}

inline bool parseNumbers(const char *str, double *output, int count) {
  if (!str) {
    return false;
  }
  for (int i = 0; i < count; ++i) {
    char *end {};
    output[i] = std::strtod(str, &end);
    if (end == str || !std::isfinite(output[i]) || (i < count - 1 && *end != ',')) {
      return false;
    }
    str = end;
    if (i < count - 1) {
      ++str;
    }
  }
  return *str == '\0' || (*str == '\n' && str[1] == '\0')
    || (*str == '\r' && (str[1] == '\0' || (str[1] == '\n' && str[2] == '\0')));
}

inline bool integerIn(double input, unsigned maximum) {
  return input >= 0.0 && input <= maximum && std::floor(input) == input;
}

inline bool formatMetadata(char *buffer, std::size_t size, const TraceMetadata &meta) {
  if (!validMetadata(meta)) {
    return false;
  }
  const auto &s = meta.initial;
  const int written = std::snprintf(buffer, size,
    "aipb-explorer-v2,%s,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.9g,%.9g,%u,%u,%.9g\n",
    meta.map, s.x, s.y, s.z, s.vx, s.vy, s.vz, s.yaw, s.pitch,
    static_cast<unsigned>(s.on_ground), static_cast<unsigned>(s.crouching), meta.viewZ);
  return written > 0 && static_cast<std::size_t>(written) < size;
}

inline bool parseMetadata(const char *line, TraceMetadata &result) {
  constexpr const char prefix[] = "aipb-explorer-v2,";
  if (!line || std::strncmp(line, prefix, sizeof(prefix) - 1) != 0) {
    return false;
  }
  const char *map = line + sizeof(prefix) - 1;
  const char *comma = std::strchr(map, ',');
  if (!comma || comma == map || static_cast<std::size_t>(comma - map) >= sizeof(result.map)) {
    return false;
  }
  TraceMetadata next {};
  std::memcpy(next.map, map, static_cast<std::size_t>(comma - map));
  if (!safeName(next.map, 63)) {
    return false;
  }
  double v[11] {};
  if (!parseNumbers(comma + 1, v, 11) || !integerIn(v[8], 1) || !integerIn(v[9], 1)) {
    return false;
  }
  auto &s = next.initial;
  s.x = v[0]; s.y = v[1]; s.z = v[2];
  s.vx = v[3]; s.vy = v[4]; s.vz = v[5];
  s.yaw = static_cast<float>(v[6]);
  s.pitch = static_cast<float>(v[7]);
  s.on_ground = static_cast<uint8_t>(v[8]);
  s.crouching = static_cast<uint8_t>(v[9]);
  next.viewZ = static_cast<float>(v[10]);
  if (!validMetadata(next)) {
    return false;
  }
  result = next;
  return true;
}

inline constexpr const char kTraceCsvColumns[] =
  "step,msec,forward,strafe,yaw_delta,jump,duck,max_speed,old_buttons,"
  "bx,by,bz,bvx,bvy,bvz,byaw,bpitch,bground,bduck,"
  "ax,ay,az,avx,avy,avz,ayaw,apitch,aground,aduck\n";

inline bool formatSample(char *buffer, std::size_t size, int step, const TraceSample &t) {
  if (step < 0 || t.msec == 0 || !valid(t.command) || !validState(t.before) || !validState(t.after)
    || !std::isfinite(t.maxSpeed) || t.maxSpeed < 0.0f) {
    return false;
  }
  const auto &b = t.before;
  const auto &a = t.after;
  const int written = std::snprintf(buffer, size,
    "%d,%u,%.9g,%.9g,%.9g,%u,%u,%.9g,%d,"
    "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.9g,%.9g,%u,%u,"
    "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.9g,%.9g,%u,%u\n",
    step, static_cast<unsigned>(t.msec),
    t.command.forward, t.command.strafe, t.command.yaw_delta,
    static_cast<unsigned>(t.command.jump), static_cast<unsigned>(t.command.duck),
    t.maxSpeed, t.oldButtons,
    b.x,b.y,b.z,b.vx,b.vy,b.vz,b.yaw,b.pitch,
    static_cast<unsigned>(b.on_ground), static_cast<unsigned>(b.crouching),
    a.x,a.y,a.z,a.vx,a.vy,a.vz,a.yaw,a.pitch,
    static_cast<unsigned>(a.on_ground), static_cast<unsigned>(a.crouching));
  return written > 0 && static_cast<std::size_t>(written) < size;
}

inline bool parseSample(const char *line, int expectedStep, TraceSample &result) {
  double v[29] {};
  if (!parseNumbers(line, v, 29)
    || !integerIn(v[0], ProbeBudget::kMaxSafetyFrames)
    || static_cast<int>(v[0]) != expectedStep || !integerIn(v[1], 255) || v[1] < 1
    || !integerIn(v[5], 1) || !integerIn(v[6], 1)
    || v[8] < static_cast<double>(std::numeric_limits<int>::min())
    || v[8] > static_cast<double>(std::numeric_limits<int>::max()) || std::floor(v[8]) != v[8]
    || !integerIn(v[17], 1) || !integerIn(v[18], 1)
    || !integerIn(v[27], 1) || !integerIn(v[28], 1)) {
    return false;
  }
  TraceSample t {};
  t.command = { static_cast<float>(v[2]), static_cast<float>(v[3]), static_cast<float>(v[4]),
    static_cast<uint8_t>(v[5]), static_cast<uint8_t>(v[6]) };
  t.maxSpeed = static_cast<float>(v[7]);
  t.oldButtons = static_cast<int>(v[8]);
  t.msec = static_cast<uint8_t>(v[1]);
  auto &b = t.before; auto &a = t.after;
  b.x=v[9]; b.y=v[10]; b.z=v[11]; b.vx=v[12]; b.vy=v[13]; b.vz=v[14];
  b.yaw=static_cast<float>(v[15]); b.pitch=static_cast<float>(v[16]);
  b.on_ground=static_cast<uint8_t>(v[17]); b.crouching=static_cast<uint8_t>(v[18]);
  a.x=v[19]; a.y=v[20]; a.z=v[21]; a.vx=v[22]; a.vy=v[23]; a.vz=v[24];
  a.yaw=static_cast<float>(v[25]); a.pitch=static_cast<float>(v[26]);
  a.on_ground=static_cast<uint8_t>(v[27]); a.crouching=static_cast<uint8_t>(v[28]);
  b.tick = static_cast<uint64_t>(expectedStep);
  a.tick = b.tick + 1;
  if (!valid(t.command) || !validState(b) || !validState(a)
    || !std::isfinite(t.maxSpeed) || t.maxSpeed < 0.0f) {
    return false;
  }
  result = t;
  return true;
}

} // namespace aipb::explorer::bridge
