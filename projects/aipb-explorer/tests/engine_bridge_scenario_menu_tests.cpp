#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include <aipb/explorer/engine_bridge_scenario_menu.h>

using namespace aipb::explorer::scenario;

static void check(bool condition) {
  if (!condition) {
    throw std::runtime_error("scenario menu regression");
  }
}

int main() {
  try {
    check(scenarioMenuAvailability(false).start);
    check(!scenarioMenuAvailability(false).stop);
    check(!scenarioMenuAvailability(true).start);
    check(scenarioMenuAvailability(true).stop);
    check(scenarioMenuOverlayText(false, "abc") == "Сценарий не запущен");
    check(scenarioMenuOverlayText(true, "ground_forward_01")
      == "Сценарий ground_forward_01 выполняется");

    ScenarioMenuBrowser browser {};
    auto empty = browser.currentPage();
    check(empty.current == 1 && empty.total == 1);
    check(!empty.next && !empty.previous);
    check(!browser.previousPage() && !browser.nextPage());
    check(!browser.back());
    check(browser.selectedScenario(0).empty());

    std::vector<ScenarioMenuEntry> entries {
      { "zeta", false },
      { "alpha", false },
      { "folder_b", true },
      { "charlie", false },
      { "folder_a", true },
      { "echo", false },
      { "delta", false },
      { "bravo", false },
      { "foxtrot", false },
      { "golf", false },
      { "hotel", false }
    };
    check(browser.setEntries(entries));
    auto page = browser.currentPage();
    check(page.current == 1 && page.total == 3);
    check(page.next && !page.previous && page.entries.size() == 5);
    check(page.entries[0].folder && page.entries[0].name == "folder_a");
    check(page.entries[1].folder && page.entries[1].name == "folder_b");
    check(browser.selectedScenario(0).empty());
    check(browser.selectedScenario(2) == "alpha");

    check(browser.nextPage());
    page = browser.currentPage();
    check(page.current == 2 && page.total == 3);
    check(page.previous && page.next && page.entries.size() == 5);
    check(browser.nextPage());
    page = browser.currentPage();
    check(page.current == 3 && page.total == 3);
    check(page.previous && !page.next && page.entries.size() == 1);
    check(!browser.nextPage());
    check(browser.selectedScenario(0) == "zeta");
    check(browser.selectedScenario(1).empty());
    check(browser.previousPage());
    check(browser.previousPage());
    check(!browser.previousPage());

    check(browser.enterFolder(0));
    check(browser.directory() == "folder_a");
    check(browser.setEntries({ { "example", false }, { "nested", true } }));
    check(browser.selectedScenario(0).empty());
    check(browser.selectedScenario(1) == "folder_a/example");
    check(browser.enterFolder(0));
    check(browser.directory() == "folder_a/nested");
    check(browser.setEntries({ { "deep", false } }));
    check(browser.selectedScenario(0) == "folder_a/nested/deep");
    check(browser.back() && browser.directory() == "folder_a");
    check(browser.back() && browser.directory().empty());
    check(!browser.back());

    check(browser.setEntries({ { "valid", false } }));
    check(!browser.setEntries({ { "../invalid", true } }));
    check(browser.selectedScenario(0) == "valid");
    check(!browser.setEntries({ { "duplicate", false }, { "duplicate", false } }));
    check(!browser.setEntries({ { "x/y", false } }));
    check(!browser.setEntries(std::vector<ScenarioMenuEntry>(
      kScenarioMenuMaxEntries + 1, { "x", false })));
    check(browser.selectedScenario(0) == "valid");

    check(browser.setEntries({ { "one", false }, { "one", true } }));
    check(browser.currentPage().entries.size() == 2);

    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "scenario menu: %s\n", e.what());
    return 1;
  }
}
