#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <aipb/explorer/engine_bridge_scenario.h>

namespace aipb::explorer::scenario {

inline constexpr std::size_t kScenarioMenuPageSize = 5;
inline constexpr std::size_t kScenarioMenuMaxEntries = 256;
inline constexpr std::size_t kScenarioMenuMaxDepth = 8;

struct ScenarioMenuEntry {
  std::string name {};
  bool folder {};
};

struct ScenarioMenuPage {
  std::size_t current { 1 };
  std::size_t total { 1 };
  bool previous {};
  bool next {};
  std::vector<ScenarioMenuEntry> entries {};
};

struct ScenarioMenuAvailability {
  bool toggleOverlay { true };
  bool start {};
  bool stop {};
};

inline ScenarioMenuAvailability scenarioMenuAvailability(bool running) {
  return { true, !running, running };
}

inline std::string scenarioMenuOverlayText(
  bool running,
  std::string_view scenarioName
) {
  if (!running) {
    return "Сценарий не запущен";
  }

  return "Сценарий " + std::string { scenarioName } + " выполняется";
}

// Every path component must remain a bounded YaPB scenario identifier.
// Listing and filesystem access are separate responsibilities of the adapter.
class ScenarioMenuBrowser {
public:
  const std::string &directory() const {
    return directory_;
  }

  std::size_t page() const {
    return page_;
  }

  bool setEntries(std::vector<ScenarioMenuEntry> entries) {
    if (entries.size() > kScenarioMenuMaxEntries) {
      return false;
    }

    for (const auto &entry : entries) {
      if (!safeIdentifier(entry.name)) {
        return false;
      }
    }

    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
      if (a.folder != b.folder) {
        return a.folder;
      }

      return a.name < b.name;
    });

    for (std::size_t i = 1; i < entries.size(); ++i) {
      if (entries[i - 1].folder == entries[i].folder
        && entries[i - 1].name == entries[i].name) {
        return false;
      }
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

    const std::size_t begin = page_ * kScenarioMenuPageSize;
    const std::size_t end = std::min(begin + kScenarioMenuPageSize, entries_.size());

    for (std::size_t i = begin; i < end; ++i) {
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

  bool enterFolder(std::size_t visibleIndex) {
    const auto *entry = visibleEntry(visibleIndex);

    if (!entry || !entry->folder || depth_ >= kScenarioMenuMaxDepth) {
      return false;
    }

    const std::string next = directory_.empty()
      ? entry->name : directory_ + "/" + entry->name;

    if (next.size() > 255) {
      return false;
    }

    directory_ = next;
    ++depth_;
    page_ = 0;
    entries_.clear();
    return true;
  }

  // Return a validated relative identifier. The runtime must separately
  // verify the selected file before calling the scenario loader.
  std::string selectedScenario(std::size_t visibleIndex) const {
    const auto *entry = visibleEntry(visibleIndex);

    if (!entry || entry->folder) {
      return {};
    }

    return directory_.empty()
      ? entry->name : directory_ + "/" + entry->name;
  }

  bool back() {
    if (depth_ == 0) {
      return false;
    }

    const auto last = directory_.rfind('/');

    if (last == std::string::npos) {
      directory_.clear();
    }
    else {
      directory_.erase(last);
    }

    --depth_;
    page_ = 0;
    entries_.clear();
    return true;
  }

private:
  std::size_t pageCount() const {
    return std::max<std::size_t>(
      1, (entries_.size() + kScenarioMenuPageSize - 1) / kScenarioMenuPageSize);
  }

  const ScenarioMenuEntry *visibleEntry(std::size_t index) const {
    if (index >= kScenarioMenuPageSize) {
      return nullptr;
    }

    const std::size_t offset = page_ * kScenarioMenuPageSize + index;
    return offset < entries_.size() ? &entries_[offset] : nullptr;
  }

  std::string directory_ {};
  std::vector<ScenarioMenuEntry> entries_ {};
  std::size_t page_ {};
  std::size_t depth_ {};
};

} // namespace aipb::explorer::scenario
