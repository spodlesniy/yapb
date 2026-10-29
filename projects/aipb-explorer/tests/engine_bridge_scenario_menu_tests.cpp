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
    check(scenarioMenuOverlayText(false, "abc") == "No scenario running");
    check(scenarioMenuOverlayText(true, "ground_forward_01")
      == "Scenario ground_forward_01 is running");
    check(scenarioMenuRootKeyMask(false) == ((1u << 0) | (1u << 1) | (1u << 9)));
    check(scenarioMenuRootKeyMask(true) == ((1u << 0) | (1u << 2) | (1u << 9)));
    check((scenarioMenuRootKeyMask(false) & (1u << 2)) == 0u);
    check((scenarioMenuRootKeyMask(true) & (1u << 1)) == 0u);

    // An open root must update on both start and completion, even with HUD off.
    check(!scenarioMenuNeedsRefresh(ScenarioMenuScreen::Closed, true, false));
    check(!scenarioMenuNeedsRefresh(ScenarioMenuScreen::Root, false, false));
    check(!scenarioMenuNeedsRefresh(ScenarioMenuScreen::Root, true, true));
    check(scenarioMenuNeedsRefresh(ScenarioMenuScreen::Root, false, true));
    check(scenarioMenuNeedsRefresh(ScenarioMenuScreen::Root, true, false));
    check(!scenarioMenuNeedsRefresh(ScenarioMenuScreen::Browser, false, false));
    check(scenarioMenuNeedsRefresh(ScenarioMenuScreen::Browser, false, true));

    check(!scenarioMenuRoundExpired(ScenarioMenuScreen::Closed, 4, 5));
    check(!scenarioMenuRoundExpired(ScenarioMenuScreen::Root, 4, 4));
    check(!scenarioMenuRoundExpired(ScenarioMenuScreen::Browser, 4, 4));
    check(scenarioMenuRoundExpired(ScenarioMenuScreen::Root, 4, 5));
    check(scenarioMenuRoundExpired(ScenarioMenuScreen::Browser, 4, 5));
    check(scenarioMenuRoundExpired(ScenarioMenuScreen::Root, 0, 1));
    check(scenarioMenuRoundExpired(ScenarioMenuScreen::Root, UINT32_MAX, 1));

    ScenarioMenuSession session {};
    check(!session.ownsSelection(false));
    session.showRoot();
    check(session.screen() == ScenarioMenuScreen::Root);
    check(session.ownsSelection(false));
    session.showBrowser();
    check(session.screen() == ScenarioMenuScreen::Browser);
    check(!session.ownsSelection(true));
    check(session.screen() == ScenarioMenuScreen::Closed);
    session.showRoot();
    session.release();
    check(!session.ownsSelection(false));
    ScenarioMenuSession other {};
    other.showBrowser();
    check(!session.ownsSelection(false));
    check(other.ownsSelection(false));

    ScenarioMenuBrowser browser {};
    auto empty = browser.currentPage();
    check(empty.current == 1 && empty.total == 1);
    check(!empty.next && !empty.previous && empty.entries.empty());
    check(!browser.previousPage() && !browser.nextPage());
    check(browser.selectedScenario(0).empty());

    check(browser.setEntries({
      "zeta", "alpha", "charlie", "echo", "delta",
      "bravo", "foxtrot", "golf", "hotel", "india", "juliet"
    }));
    auto page = browser.currentPage();
    check(page.current == 1 && page.total == 3);
    check(page.next && !page.previous && page.entries.size() == 5);
    check(page.entries[0] == "alpha" && browser.selectedScenario(4) == "echo");
    check(browser.nextPage());
    page = browser.currentPage();
    check(page.current == 2 && page.total == 3);
    check(page.previous && page.next && page.entries.size() == 5);
    check(page.entries[0] == "foxtrot");
    check(browser.nextPage());
    page = browser.currentPage();
    check(page.current == 3 && page.total == 3);
    check(page.previous && !page.next && page.entries.size() == 1);
    check(!browser.nextPage());
    check(browser.selectedScenario(0) == "zeta");
    check(browser.selectedScenario(1).empty());
    check(browser.previousPage() && browser.previousPage());
    check(!browser.previousPage());

    check(browser.setEntries({ "valid" }));
    check(!browser.setEntries({ "../invalid" }));
    check(!browser.setEntries({ "x/y" }));
    check(!browser.setEntries({ "duplicate", "duplicate" }));
    check(!browser.setEntries(std::vector<std::string>(
      kScenarioMenuMaxEntries + 1, "x")));
    check(browser.selectedScenario(0) == "valid");
    check(browser.setEntries({}));
    check(browser.currentPage().total == 1);

    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "scenario menu: %s\n", e.what());
    return 1;
  }
}
