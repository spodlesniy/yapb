#include <cstdio>
#include <limits>
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
    check(scenarioMenuOverlayPoseText(false, "ignored",
      448.0, 2368.0, -92.0, -105.0, 0.0)
      == "No scenario running\nPos: (448.000, 2368.000, -92.000)"
         "\nView: yaw=-105.00 pitch=0.00");
    check(scenarioMenuOverlayPoseText(true, "ground_slope_down_01",
      876.4186, 2247.1721, -0.96786, -180.0, -7.5)
      == "Scenario ground_slope_down_01 is running"
         "\nPos: (876.419, 2247.172, -0.968)"
         "\nView: yaw=-180.00 pitch=-7.50");
    check(scenarioMenuOverlayPoseText(false, "",
      1.0, 2.0, 3.0, 0.0, 0.0)
      != scenarioMenuOverlayPoseText(false, "",
      2.0, 2.0, 3.0, 0.0, 0.0));
    check(scenarioMenuOverlayPoseText(false, "",
      0.0, 0.0, 0.0, 0.0, 0.0).find("Pos:") != std::string::npos);
    check(scenarioMenuOverlayPoseText(false, "",
      std::numeric_limits<double>::quiet_NaN(), 0, 0, 0, 0)
      == "No scenario running\nPos: unavailable\nView: unavailable");
    check(scenarioMenuAvailability(false).moveBot);
    check(!scenarioMenuAvailability(true).moveBot);
    check(scenarioMenuRootKeyMask(false) == ((1u << 0) | (1u << 1)
      | (1u << 2) | (1u << 9)));
    check(scenarioMenuRootKeyMask(true) == ((1u << 0) | (1u << 3) | (1u << 9)));
    check((scenarioMenuRootKeyMask(false) & (1u << 3)) == 0u);
    check((scenarioMenuRootKeyMask(true) & (1u << 1)) == 0u);
    check((scenarioMenuRootKeyMask(true) & (1u << 2)) == 0u);

    ScenarioMenuTargetPose moved {};
    check(scenarioMenuRoundedPose(448.49, -2368.5, -91.51, -104.5, 7.5, moved));
    check(moved.position[0] == 448.0f && moved.position[1] == -2369.0f);
    check(moved.position[2] == -92.0f && moved.yaw == -105.0f);
    check(moved.pitch == 8.0f);
    check(!scenarioMenuRoundedPose(
      std::numeric_limits<double>::quiet_NaN(), 0, 0, 0, 0, moved));
    check(!scenarioMenuRoundedPose(20000, 0, 0, 0, 0, moved));
    check(!scenarioMenuRoundedPose(0, 0, 0, 0,
      std::numeric_limits<double>::infinity(), moved));
    check(moved.position[0] == 448.0f && moved.yaw == -105.0f);

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
