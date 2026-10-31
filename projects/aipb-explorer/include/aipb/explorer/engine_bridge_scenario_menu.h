#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_scenario.h>

namespace aipb::explorer::scenario {

inline constexpr std::size_t kScenarioMenuPageSize = 5;
inline constexpr std::size_t kScenarioMenuMaxEntries = 256;

enum class ScenarioMenuScreen { Closed, Root, Browser };

// An AiPB menu must not survive into the next game round.
inline bool scenarioMenuRoundExpired(
  ScenarioMenuScreen screen,
  std::uint32_t displayedRoundId,
  std::uint32_t currentRoundId
) {
  return screen != ScenarioMenuScreen::Closed && displayedRoundId != currentRoundId;
}

// Refresh an owned menu only when its action availability has changed.
inline bool scenarioMenuNeedsRefresh(
  ScenarioMenuScreen screen,
  bool renderedRunning,
  bool running
) {
  return (screen == ScenarioMenuScreen::Root && renderedRunning != running)
    || (screen == ScenarioMenuScreen::Browser && running);
}

// Ownership ends when another menu becomes active.
class ScenarioMenuSession {
public:
  void showRoot() { screen_ = ScenarioMenuScreen::Root; }
  void showBrowser() { screen_ = ScenarioMenuScreen::Browser; }
  void release() { screen_ = ScenarioMenuScreen::Closed; }
  ScenarioMenuScreen screen() const { return screen_; }
  bool ownsSelection(bool otherMenuActive) {
    if (otherMenuActive) {
      release();
    }
    return screen_ != ScenarioMenuScreen::Closed;
  }

private:
  ScenarioMenuScreen screen_ { ScenarioMenuScreen::Closed };
};

struct ScenarioMenuPage {
  std::size_t current { 1 };
  std::size_t total { 1 };
  bool previous {};
  bool next {};
  std::vector<std::string> entries {};
};

struct ScenarioMenuAvailability {
  bool toggleOverlay { true };
  bool moveBot {};
  bool start {};
  bool stop {};
};

inline ScenarioMenuAvailability scenarioMenuAvailability(bool running) {
  return { true, !running, !running, running };
}

inline unsigned int scenarioMenuRootKeyMask(bool running) {
  const auto availability = scenarioMenuAvailability(running);

  return (1u << 0) | (1u << 9)
    | (availability.moveBot ? (1u << 1) : 0u)
    | (availability.start ? (1u << 2) : 0u)
    | (availability.stop ? (1u << 3) : 0u);
}

struct ScenarioMenuTargetPose {
  float position[3] {};
  float yaw {};
  float pitch {};
};

// Round live client coordinates and view angles before the engine adapter teleports.
// Do not change the output if the source pose is invalid.
inline bool scenarioMenuRoundedPose(
  double x,
  double y,
  double z,
  double yaw,
  double pitch,
  ScenarioMenuTargetPose &result
) {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
      || !std::isfinite(yaw) || !std::isfinite(pitch)
      || std::abs(x) > 16384.0 || std::abs(y) > 16384.0
      || std::abs(z) > 16384.0 || std::abs(yaw) > 3600.0
      || std::abs(pitch) > 3600.0) {
    return false;
  }

  ScenarioMenuTargetPose next {};
  next.position[0] = static_cast<float>(std::round(x));
  next.position[1] = static_cast<float>(std::round(y));
  next.position[2] = static_cast<float>(std::round(z));
  next.yaw = static_cast<float>(std::round(yaw));
  next.pitch = static_cast<float>(std::round(pitch));
  result = next;
  return true;
}

inline std::string scenarioMenuOverlayText(
  bool running,
  std::string_view scenarioName
) {
  if (!running) {
    return "No scenario running";
  }

  return "Scenario " + std::string { scenarioName } + " is running";
}

// Produce a dedicated HUD payload per recipient; never display another player's pose.
inline std::string scenarioMenuOverlayPoseText(
  bool running,
  std::string_view scenarioName,
  double x,
  double y,
  double z,
  double yaw,
  double pitch
) {
  const std::string status = scenarioMenuOverlayText(running, scenarioName);
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
      || !std::isfinite(yaw) || !std::isfinite(pitch)) {
    return status + "\nPos: unavailable\nView: unavailable";
  }

  char pose[160] {};
  const int length = std::snprintf(pose, sizeof(pose),
    "\nPos: (%.3f, %.3f, %.3f)\nView: yaw=%.2f pitch=%.2f",
    x, y, z, yaw, pitch);
  if (length < 0 || static_cast<std::size_t>(length) >= sizeof(pose)) {
    return status + "\nPos: unavailable\nView: unavailable";
  }

  return status + pose;
}

// Flat, deterministic browser. Only .scenario file basenames are accepted.
// Enumeration, authorization and selection validation belong to the adapter.
class ScenarioMenuBrowser {
public:
  bool setEntries(std::vector<std::string> entries) {
    if (entries.size() > kScenarioMenuMaxEntries) {
      return false;
    }

    for (const auto &name : entries) {
      if (!safeIdentifier(name)) {
        return false;
      }
    }

    std::sort(entries.begin(), entries.end());

    if (std::adjacent_find(entries.begin(), entries.end()) != entries.end()) {
      return false;
    }

    entries_ = std::move(entries);
    page_ = 0;
    return true;
  }

  ScenarioMenuPage currentPage() const {
    ScenarioMenuPage result {};
    result.current = page_ + 1;
    result.total = pageCount();
    result.previous = page_ > 0;
    result.next = page_ + 1 < result.total;

    const std::size_t first = page_ * kScenarioMenuPageSize;
    const std::size_t last = std::min(first + kScenarioMenuPageSize, entries_.size());

    for (std::size_t i = first; i < last; ++i) {
      result.entries.push_back(entries_[i]);
    }

    return result;
  }

  bool previousPage() {
    if (page_ == 0) {
      return false;
    }

    --page_;
    return true;
  }

  bool nextPage() {
    if (page_ + 1 >= pageCount()) {
      return false;
    }

    ++page_;
    return true;
  }

  std::string selectedScenario(std::size_t visibleIndex) const {
    if (visibleIndex >= kScenarioMenuPageSize) {
      return {};
    }

    const std::size_t absoluteIndex = page_ * kScenarioMenuPageSize + visibleIndex;
    return absoluteIndex < entries_.size() ? entries_[absoluteIndex] : std::string {};
  }

private:
  std::size_t pageCount() const {
    return std::max<std::size_t>(
      1, (entries_.size() + kScenarioMenuPageSize - 1) / kScenarioMenuPageSize);
  }

  std::vector<std::string> entries_ {};
  std::size_t page_ {};
};

} // namespace aipb::explorer::scenario
