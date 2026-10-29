#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include <aipb/explorer/engine_bridge_scenario.h>

namespace aipb::explorer::scenario {

enum class LoadStatus {
  Ok,
  InvalidName,
  NotFoundOrUnreadable,
  TooLarge,
  InvalidFormat
};

inline const char *loadStatusName(LoadStatus status) {
  switch (status) {
  case LoadStatus::Ok: return "Ok";
  case LoadStatus::InvalidName: return "InvalidName";
  case LoadStatus::NotFoundOrUnreadable: return "NotFoundOrUnreadable";
  case LoadStatus::TooLarge: return "TooLarge";
  case LoadStatus::InvalidFormat: return "InvalidFormat";
  }

  return "InvalidFormat";
}

// The trusted root is the YaPB data/explorer directory. File names never
// contain path separators; the loader neither creates files nor executes code.
inline bool scenarioPath(
  std::string_view explorerDirectory,
  std::string_view name,
  std::string &result
) {
  if (explorerDirectory.empty() || !safeIdentifier(name)
    || explorerDirectory.size() > 4096) {
    return false;
  }

  std::string path { explorerDirectory };
  const char last = path.back();

  if (last != '/' && last != '\\') {
    path += '/';
  }

  path += "scenarios/";
  path.append(name.data(), name.size());
  path += ".scenario";
  result = std::move(path);
  return true;
}

// Bounded binary reading avoids silently accepting truncated scenarios.
// The parse result remains untouched on any file or validation failure.
inline LoadStatus loadScenario(
  std::string_view explorerDirectory,
  std::string_view name,
  Scenario &result,
  std::string &detail
) {
  std::string path {};

  if (!scenarioPath(explorerDirectory, name, path)) {
    detail = "invalid scenario name or directory";
    return LoadStatus::InvalidName;
  }

  std::FILE *file = std::fopen(path.c_str(), "rb");

  if (!file) {
    detail = "scenario file missing or unreadable";
    return LoadStatus::NotFoundOrUnreadable;
  }

  std::string content {};
  char buffer[4096] {};
  bool tooLarge = false;
  bool readError = false;

  for (;;) {
    const std::size_t count = std::fread(buffer, 1, sizeof(buffer), file);

    if (count > kMaxFileBytes - content.size()) {
      tooLarge = true;
      break;
    }

    content.append(buffer, count);

    if (count < sizeof(buffer)) {
      readError = std::ferror(file) != 0;
      break;
    }
  }

  std::fclose(file);

  if (tooLarge) {
    detail = "scenario exceeds 65536 bytes";
    return LoadStatus::TooLarge;
  }

  if (readError) {
    detail = "scenario read failed";
    return LoadStatus::NotFoundOrUnreadable;
  }

  Scenario parsed {};
  std::string parserError {};

  if (!parseScenario(content, name, parsed, parserError)) {
    detail = std::move(parserError);
    return LoadStatus::InvalidFormat;
  }

  result = std::move(parsed);
  detail.clear();
  return LoadStatus::Ok;
}

enum class PreflightStatus {
  Ok,
  MapMismatch,
  BotMissing,
  BotDead,
  TeamMismatch
};

inline const char *preflightStatusName(PreflightStatus status) {
  switch (status) {
  case PreflightStatus::Ok: return "Ok";
  case PreflightStatus::MapMismatch: return "MapMismatch";
  case PreflightStatus::BotMissing: return "BotMissing";
  case PreflightStatus::BotDead: return "BotDead";
  case PreflightStatus::TeamMismatch: return "TeamMismatch";
  }

  return "BotMissing";
}

// Read-only preflight: position, weapon and equipment application will have
// their own guarded runtime step; validation cannot authorize movement.
inline PreflightStatus checkScenarioPreflight(
  const Scenario &scenario,
  std::string_view currentMap,
  bool botPresent,
  bool botAlive,
  int currentTeam
) {
  if (scenario.map != currentMap) {
    return PreflightStatus::MapMismatch;
  }

  if (!botPresent) {
    return PreflightStatus::BotMissing;
  }

  if (!botAlive) {
    return PreflightStatus::BotDead;
  }

  if (currentTeam != scenario.team) {
    return PreflightStatus::TeamMismatch;
  }

  return PreflightStatus::Ok;
}

} // namespace aipb::explorer::scenario
