#pragma once

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <set>
#include <string>
#include <system_error>
#include <string_view>
#include <utility>
#include <vector>

namespace aipb::explorer::scenario {

inline constexpr std::string_view kVersion = "aipb-scenario-v1";
inline constexpr int kMaxActions = 32;
inline constexpr int kMaxScenarioMs = 60000;
inline constexpr int kMaxActionMs = 10000;
inline constexpr int kMaxActionTimeoutMs = kMaxActionMs + (kMaxActionMs + 9) / 10;
inline constexpr std::size_t kMaxFileBytes = 65536;

struct Weapon {
  std::string type { "none" };
  int clip {};
  int reserve {};
};

struct Equipment {
  Weapon primary {};
  Weapon secondary {};
  int he {};
  int flash {};
  int smoke {};
  bool defuseKit {};
  int money {};
  int health { 100 };
  int armor {};
  bool helmet {};
  std::string selected { "knife" };
  std::set<std::string> specified {};

  bool has(std::string_view key) const {
    return specified.count(std::string { key }) != 0;
  }
};

struct Action {
  int durationMs {};
  int timeoutMs {};
  float forward {};
  float strafe {};
  float yawDelta {};
  float pitchDelta {};
  bool jump {};
  bool duck {};
  bool attack {};
  bool attack2 {};
  bool use {};
  std::string select { "none" };
};

struct Scenario {
  std::string name {};
  std::string map {};
  int timeoutMs {};
  int botIndex { -1 };
  int team {};
  double position[3] {};
  float yaw {};
  float pitch {};
  float velocity[3] {};
  bool crouching {};
  std::set<std::string> specifiedBot {};

  bool hasBot(std::string_view key) const {
    return specifiedBot.count(std::string { key }) != 0;
  }

  Equipment equipment {};
  std::vector<Action> actions {};
};

inline bool safeIdentifier(std::string_view value) {
  if (value.empty() || value.size() > 63) {
    return false;
  }

  for (const char c : value) {
    const bool allowed = (c >= 'a' && c <= 'z')
      || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
      || c == '_' || c == '-';

    if (!allowed) {
      return false;
    }
  }

  return true;
}

inline bool knownWeapon(std::string_view name, bool primary) {
  constexpr std::string_view primaries[] {
    "none", "m3", "xm1014", "mac10", "tmp", "mp5", "ump45", "p90",
    "galil", "famas", "ak47", "m4a1", "sg552", "aug", "scout",
    "awp", "g3sg1", "sg550", "m249"
  };
  constexpr std::string_view secondaries[] {
    "none", "glock18", "usp", "p228", "deagle", "fiveseven", "elite"
  };

  if (primary) {
    for (const auto value : primaries) {
      if (name == value) {
        return true;
      }
    }
  }
  else {
    for (const auto value : secondaries) {
      if (name == value) {
        return true;
      }
    }
  }

  return false;
}

inline bool validAction(const Action &action) {
  const auto normalized = [](float value) {
    return std::isfinite(value) && value >= -1.0f && value <= 1.0f;
  };

  return action.durationMs > 0 && action.durationMs <= kMaxActionMs
    && action.timeoutMs >= action.durationMs
    && action.timeoutMs <= kMaxActionTimeoutMs
    && normalized(action.forward) && normalized(action.strafe)
    && std::isfinite(action.yawDelta) && action.yawDelta >= -180.0f
    && action.yawDelta <= 180.0f
    && std::isfinite(action.pitchDelta) && action.pitchDelta >= -180.0f
    && action.pitchDelta <= 180.0f
    && (action.select == "none" || action.select == "knife"
      || action.select == "primary" || action.select == "secondary"
      || action.select == "he" || action.select == "flash"
      || action.select == "smoke");
}

inline bool validScenario(const Scenario &scenario) {
  const auto &e = scenario.equipment;

  if (!safeIdentifier(scenario.name) || !safeIdentifier(scenario.map)
    || scenario.botIndex < 0 || scenario.botIndex > 31
    || (scenario.team != 0 && scenario.team != 1 && scenario.team != 2)
    || scenario.timeoutMs < 1 || scenario.timeoutMs > kMaxScenarioMs
    || !std::isfinite(scenario.yaw) || scenario.yaw < -180.0f
    || scenario.yaw >= 180.0f || !std::isfinite(scenario.pitch)
    || scenario.pitch < -89.0f || scenario.pitch > 89.0f
    || !knownWeapon(e.primary.type, true)
    || !knownWeapon(e.secondary.type, false)
    || e.primary.clip < 0 || e.primary.clip > 250
    || e.primary.reserve < 0 || e.primary.reserve > 250
    || e.secondary.clip < 0 || e.secondary.clip > 250
    || e.secondary.reserve < 0 || e.secondary.reserve > 250
    || (e.primary.type == "none" && (e.primary.clip != 0 || e.primary.reserve != 0))
    || (e.secondary.type == "none" && (e.secondary.clip != 0 || e.secondary.reserve != 0))
    || e.he < 0 || e.he > 1 || e.flash < 0 || e.flash > 2
    || e.smoke < 0 || e.smoke > 1 || (e.defuseKit && scenario.team != 2)
    || e.money < 0 || e.money > 16000
    || e.health < 1 || e.health > 100 || e.armor < 0 || e.armor > 100
    || (e.has("helmet") && e.helmet && e.has("armor") && e.armor == 0)
    || ((e.has("primary_clip") || e.has("primary_reserve")) && !e.has("primary"))
    || ((e.has("secondary_clip") || e.has("secondary_reserve")) && !e.has("secondary"))
    || !(e.selected == "knife"
      || (e.selected == "primary" && e.primary.type != "none")
      || (e.selected == "secondary" && e.secondary.type != "none")
      || (e.selected == "he" && e.he > 0)
      || (e.selected == "flash" && e.flash > 0)
      || (e.selected == "smoke" && e.smoke > 0))
    || scenario.actions.empty() || scenario.actions.size() > kMaxActions) {
    return false;
  }

  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(scenario.position[axis])
      || std::abs(scenario.position[axis]) > 16384.0
      || !std::isfinite(scenario.velocity[axis])
      || std::abs(scenario.velocity[axis]) > 400.0f) {
      return false;
    }
  }

  int durationTotal = 0;
  int timeoutTotal = 0;

  for (const Action &action : scenario.actions) {
    if (!validAction(action)
      || (action.select == "primary" && e.primary.type == "none")
      || (action.select == "secondary" && e.secondary.type == "none")
      || (action.select == "he" && e.he == 0)
      || (action.select == "flash" && e.flash == 0)
      || (action.select == "smoke" && e.smoke == 0)) {
      return false;
    }

    durationTotal += action.durationMs;
    timeoutTotal += action.timeoutMs;

    if (durationTotal > scenario.timeoutMs || timeoutTotal > scenario.timeoutMs) {
      return false;
    }
  }

  return true;
}

inline std::string trim(std::string_view input) {
  const std::size_t first = input.find_first_not_of(" \t\r");
  if (first == std::string_view::npos) {
    return {};
  }

  const std::size_t last = input.find_last_not_of(" \t\r");
  return std::string { input.substr(first, last - first + 1) };
}

inline bool parseInteger(
  std::string_view input,
  int minimum,
  int maximum,
  int &output
) {
  int value {};
  const auto result = std::from_chars(input.data(), input.data() + input.size(), value);

  if (result.ec != std::errc {} || result.ptr != input.data() + input.size()
    || value < minimum || value > maximum) {
    return false;
  }

  output = value;
  return true;
}

inline bool parseNumber(
  const std::string &input,
  double minimum,
  double maximum,
  double &output
) {
  char *end {};
  const double value = std::strtod(input.c_str(), &end);

  if (end == input.c_str() || *end != '\0' || !std::isfinite(value)
    || value < minimum || value > maximum) {
    return false;
  }

  output = value;
  return true;
}

inline bool parseFlag(std::string_view input, bool &output) {
  if (input == "0") {
    output = false;
    return true;
  }

  if (input == "1") {
    output = true;
    return true;
  }

  return false;
}

// The parser owns no file access. The runtime must load only a validated
// scenario name from its dedicated scenario directory.
inline bool parseScenario(
  std::string_view source,
  std::string_view expectedName,
  Scenario &result,
  std::string &error
) {
  if (!safeIdentifier(expectedName) || source.size() > kMaxFileBytes) {
    error = "invalid name or file size";
    return false;
  }

  Scenario next {};
  std::set<std::string> seen {};
  std::string section {};
  bool versionSeen = false;
  std::size_t cursor = 0;
  int lineNumber = 0;

  const auto fail = [&](std::string_view message) {
    error = "line " + std::to_string(lineNumber) + ": " + std::string { message };
    return false;
  };

  while (cursor < source.size()) {
    const std::size_t end = source.find('\n', cursor);
    const std::size_t length = end == std::string_view::npos
      ? source.size() - cursor : end - cursor;
    ++lineNumber;

    if (length > 512) {
      return fail("line too long");
    }

    std::string line = trim(source.substr(cursor, length));
    cursor = end == std::string_view::npos ? source.size() : end + 1;
    const std::size_t comment = line.find('#');

    if (comment != std::string::npos) {
      line = trim(std::string_view { line }.substr(0, comment));
    }

    if (line.empty()) {
      continue;
    }

    if (line.front() == '[' && line.back() == ']') {
      section = line.substr(1, line.size() - 2);

      if (section == "action") {
        if (next.actions.size() >= kMaxActions) {
          return fail("too many actions");
        }

        next.actions.emplace_back();
      }
      else if (section != "scenario" && section != "bot"
        && section != "equipment") {
        return fail("unknown section");
      }

      continue;
    }

    const std::size_t equals = line.find('=');

    if (equals == std::string::npos || section.empty()) {
      return fail("expected section key=value");
    }

    const std::string key = trim(std::string_view { line }.substr(0, equals));
    const std::string value = trim(std::string_view { line }.substr(equals + 1));

    if (key.empty() || value.empty() || line.find('=', equals + 1) != std::string::npos) {
      return fail("invalid key/value");
    }

    const std::string identity = section + "."
      + (section == "action" ? std::to_string(next.actions.size()) + "." : "")
      + key;

    if (!seen.insert(identity).second) {
      return fail("duplicate key");
    }

    const auto integer = [&](int min, int max, int &field) {
      return parseInteger(value, min, max, field);
    };
    const auto number = [&](double min, double max, double &field) {
      return parseNumber(value, min, max, field);
    };
    const auto narrow = [&](double min, double max, float &field) {
      double parsed {};

      if (!number(min, max, parsed)) {
        return false;
      }

      field = static_cast<float>(parsed);
      return true;
    };

    bool ok = false;

    if (section == "scenario") {
      if (key == "version") {
        versionSeen = std::string_view { value } == kVersion;
        ok = versionSeen;
      }
      else if (key == "name") {
        next.name = value;
        ok = true;
      }
      else if (key == "map") {
        next.map = value;
        ok = true;
      }
      else if (key == "timeout_ms") {
        ok = integer(1, kMaxScenarioMs, next.timeoutMs);
      }
    }
    else if (section == "bot") {
      if (key == "index") { ok = integer(0, 31, next.botIndex); }
      else if (key == "team") { ok = integer(1, 2, next.team); }
      else if (key == "x") { ok = number(-16384, 16384, next.position[0]); }
      else if (key == "y") { ok = number(-16384, 16384, next.position[1]); }
      else if (key == "z") { ok = number(-16384, 16384, next.position[2]); }
      else if (key == "yaw") { ok = narrow(-180, 180, next.yaw) && next.yaw < 180; }
      else if (key == "pitch") { ok = narrow(-89, 89, next.pitch); }
      else if (key == "vx") { ok = narrow(-400, 400, next.velocity[0]); }
      else if (key == "vy") { ok = narrow(-400, 400, next.velocity[1]); }
      else if (key == "vz") { ok = narrow(-400, 400, next.velocity[2]); }
      else if (key == "crouching") { ok = parseFlag(value, next.crouching); }
    }
    else if (section == "equipment") {
      Equipment &e = next.equipment;

      if (key == "primary") { e.primary.type = value; ok = true; }
      else if (key == "primary_clip") { ok = integer(0, 250, e.primary.clip); }
      else if (key == "primary_reserve") { ok = integer(0, 250, e.primary.reserve); }
      else if (key == "secondary") { e.secondary.type = value; ok = true; }
      else if (key == "secondary_clip") { ok = integer(0, 250, e.secondary.clip); }
      else if (key == "secondary_reserve") { ok = integer(0, 250, e.secondary.reserve); }
      else if (key == "he") { ok = integer(0, 1, e.he); }
      else if (key == "flash") { ok = integer(0, 2, e.flash); }
      else if (key == "smoke") { ok = integer(0, 1, e.smoke); }
      else if (key == "defuse_kit") { ok = parseFlag(value, e.defuseKit); }
      else if (key == "money") { ok = integer(0, 16000, e.money); }
      else if (key == "health") { ok = integer(1, 100, e.health); }
      else if (key == "armor") { ok = integer(0, 100, e.armor); }
      else if (key == "helmet") { ok = parseFlag(value, e.helmet); }
      else if (key == "selected") { e.selected = value; ok = true; }
    }
    else if (section == "action" && !next.actions.empty()) {
      Action &a = next.actions.back();

      if (key == "duration_ms") { ok = integer(1, kMaxActionMs, a.durationMs); }
      else if (key == "timeout_ms") { ok = integer(1, kMaxActionTimeoutMs, a.timeoutMs); }
      else if (key == "forward") { ok = narrow(-1, 1, a.forward); }
      else if (key == "strafe") { ok = narrow(-1, 1, a.strafe); }
      else if (key == "yaw_delta") { ok = narrow(-180, 180, a.yawDelta); }
      else if (key == "pitch_delta") { ok = narrow(-180, 180, a.pitchDelta); }
      else if (key == "jump") { ok = parseFlag(value, a.jump); }
      else if (key == "duck") { ok = parseFlag(value, a.duck); }
      else if (key == "attack") { ok = parseFlag(value, a.attack); }
      else if (key == "attack2") { ok = parseFlag(value, a.attack2); }
      else if (key == "use") { ok = parseFlag(value, a.use); }
      else if (key == "select") { a.select = value; ok = true; }
    }

    if (!ok) {
      return fail("unknown key or invalid value");
    }

    if (section == "equipment") {
      next.equipment.specified.insert(key);
    }
    else if (section == "bot") {
      next.specifiedBot.insert(key);
    }
  }

  if (!versionSeen || next.name != expectedName
    || !seen.count("scenario.name")
    || !seen.count("scenario.map")
    || !seen.count("scenario.timeout_ms")
    || !seen.count("bot.index")
    || !seen.count("bot.x") || !seen.count("bot.y")
    || !seen.count("bot.z") || !seen.count("bot.yaw")
    || !seen.count("bot.pitch")) {
    return fail("missing required fields or name mismatch");
  }

  for (std::size_t i = 0; i < next.actions.size(); ++i) {
    if (!seen.count("action." + std::to_string(i + 1) + ".duration_ms")) {
      return fail("action duration is required");
    }

    if (next.actions[i].timeoutMs == 0) {
      // Round the default 10-percent wall-clock margin up to the next millisecond.
      next.actions[i].timeoutMs = next.actions[i].durationMs
        + (next.actions[i].durationMs + 9) / 10;
    }
  }

  if (!validScenario(next)) {
    return fail("scenario constraints violated");
  }

  result = std::move(next);
  error.clear();
  return true;
}

} // namespace aipb::explorer::scenario
