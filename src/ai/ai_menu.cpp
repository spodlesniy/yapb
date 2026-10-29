#include <yapb.h>
#include <ai/ai_menu.h>
#include <ai/ai_exploration_probe.h>
#include <aipb/explorer/engine_bridge_scenario_menu.h>

#include <filesystem>
#include <system_error>
#include <vector>
#include <string>
#include <cstring>

namespace ai::menu {
namespace {

constexpr int kPlayers = 32;
constexpr int kMenuChunk = 140;
struct PlayerMenu {
   aipb::explorer::scenario::ScenarioMenuSession session {};
   aipb::explorer::scenario::ScenarioMenuBrowser browser {};
};
PlayerMenu players[kPlayers] {};
bool overlayEnabled {};
float nextOverlay {};
bool sendingOwnMenu {};

struct OwnMenuTransmission {
   OwnMenuTransmission() { sendingOwnMenu = true; }
   ~OwnMenuTransmission() { sendingOwnMenu = false; }
};

bool allowed(edict_t *ent) {
   if (!game.isPlayerEntity(ent) || game.isFakeClientEntity(ent)) {
      return false;
   }
   const int index = game.indexOfPlayer(ent);
   return index >= 0 && index < kPlayers
      && (ent == game.getLocalEntity()
         || (util.getClient(index).flags & ClientFlags::Admin));
}

void send(edict_t *ent, int keys, const String &text) {
   const OwnMenuTransmission ownMessage {};
   const auto chunks = text.split(kMenuChunk);
   for (size_t i = 0; i < chunks.length(); ++i) {
      MessageWriter(MSG_ONE, msgs.id(NetMsg::ShowMenu), nullptr, ent)
         .writeShort(keys).writeChar(-1)
         .writeByte(i + 1 == chunks.length() ? HLFalse : HLTrue)
         .writeString(chunks[i].chars());
   }
   util.getClient(game.indexOfPlayer(ent)).menu = Menu::None;
}

void close(edict_t *ent) {
   const int index = game.indexOfPlayer(ent);
   if (index < 0 || index >= kPlayers) {
      return;
   }
   players[index].session.release();
   const OwnMenuTransmission ownMessage {};
   MessageWriter(MSG_ONE, msgs.id(NetMsg::ShowMenu), nullptr, ent)
      .writeShort(0).writeChar(0).writeByte(0).writeString("");
   util.getClient(index).menu = Menu::None;
}

bool loadList(aipb::explorer::scenario::ScenarioMenuBrowser &browser) {
   namespace fs = std::filesystem;
   const auto explorer = strings.joinPath(bstor.getRunningPath(), folders.data, "explorer");
   const auto root = strings.joinPath(explorer, "scenarios");
   std::error_code error {};
   fs::directory_iterator it(fs::path(root.chars()), error);
   if (error) {
      return false;
   }
   std::vector<std::string> names {};
   const fs::directory_iterator end {};
   for (; it != end && !error; it.increment(error)) {
      std::error_code statusError {};
      if (!it->is_regular_file(statusError) || statusError) {
         continue;
      }
      const auto path = it->path();
      if (path.extension() != ".scenario") {
         continue;
      }
      const auto name = path.stem().string();
      if (!aipb::explorer::scenario::safeIdentifier(name)) {
         continue;
      }
      if (names.size() >= aipb::explorer::scenario::kScenarioMenuMaxEntries) {
         return false;
      }
      names.push_back(name);
   }
   return !error && browser.setEntries(std::move(names));
}

void root(edict_t *ent) {
   const int index = game.indexOfPlayer(ent);
   auto &state = players[index];
   state.session.showRoot();
   const bool running = ai::explore::scenarioRunning();
   const auto availability = aipb::explorer::scenario::scenarioMenuAvailability(running);
   String text {};
   text.assignf("\\yAiPB Scenarios\\w\n\n1. Overlay: %s\n",
      overlayEnabled ? "ON" : "OFF");
   text.appendf("%s2. Run Scenario%s\n",
      availability.start ? "\\w" : "\\d",
      availability.start ? "" : " (disabled)");
   text.appendf("%s3. Stop Scenario%s\n\n\\w0. Exit",
      availability.stop ? "\\w" : "\\d",
      availability.stop ? "" : " (disabled)");
   const int keys = static_cast<int>(aipb::explorer::scenario::scenarioMenuRootKeyMask(running));
   send(ent, keys, text);
}

void browse(edict_t *ent, bool reload) {
   const int index = game.indexOfPlayer(ent);
   auto &state = players[index];
   if (ai::explore::scenarioRunning()) {
      root(ent);
      return;
   }
   if (reload && !loadList(state.browser)) {
      game.clientPrint(ent, "%s", "AiPB: cannot enumerate scenario files.");
      root(ent);
      return;
   }
   state.session.showBrowser();
   const auto page = state.browser.currentPage();
   String text {};
   text.assignf("\\yAiPB Scenarios (%d/%d)\\w\n\n",
      static_cast<int>(page.current), static_cast<int>(page.total));
   int keys = cr::bit(8) | cr::bit(9);
   for (size_t i = 0; i < page.entries.size(); ++i) {
      keys |= cr::bit(static_cast<int>(i));
      text.appendf("%d. %s\n", static_cast<int>(i) + 1, page.entries[i].c_str());
   }
   if (page.previous) {
      keys |= cr::bit(6);
      text.append("7. Previous Page\n");
   }
   if (page.next) {
      keys |= cr::bit(7);
      text.append("8. Next Page\n");
   }
   text.append("\n9. Back\n0. Exit");
   send(ent, keys, text);
}

void onSelect(edict_t *ent, int selected) {
   if (!allowed(ent)) {
      return;
   }
   auto &state = players[game.indexOfPlayer(ent)];
   const auto screen = state.session.screen();
   if (selected == 10 || selected == 0) {
      close(ent);
      return;
   }

   if (screen == aipb::explorer::scenario::ScenarioMenuScreen::Root) {
      const auto availability = aipb::explorer::scenario::scenarioMenuAvailability(
         ai::explore::scenarioRunning());
      if (selected == 1 && availability.toggleOverlay) {
         overlayEnabled = !overlayEnabled;
         root(ent);
      }
      else if (selected == 2 && availability.start) {
         browse(ent, true);
      }
      else if (selected == 3 && availability.stop) {
         ai::explore::stopScenario();
         root(ent);
      }
      return;
   }

   if (screen != aipb::explorer::scenario::ScenarioMenuScreen::Browser) {
      return;
   }
   if (selected >= 1 && selected <= 5) {
      const auto name = state.browser.selectedScenario(static_cast<size_t>(selected - 1));
      if (!name.empty()) {
         const bool started = ai::explore::runScenario(name.c_str());
         game.clientPrint(ent, "%s", started ? "AiPB: scenario started." :
            "AiPB: scenario rejected; see server console.");
         root(ent);
      }
   }
   else if (selected == 7 && state.browser.previousPage()) {
      browse(ent, false);
   }
   else if (selected == 8 && state.browser.nextPage()) {
      browse(ent, false);
   }
   else if (selected == 9) {
      root(ent);
   }
}

bool handleCommand(edict_t *ent, bool server) {
   const int argc = engfuncs.pfnCmd_Argc();
   const char *group = argc >= 2 ? engfuncs.pfnCmd_Argv(1) : "";
   if (std::strcmp(group, "menu") == 0 && argc == 2) {
      edict_t *target = server ? game.getLocalEntity() : ent;
      if (allowed(target)) {
         root(target);
      }
      else if (server) {
         game.print("AiPB: menu requires an authorized player client.");
      }
      return true;
   }
   if (std::strcmp(group, "scenario") != 0) {
      game.print("Usage: aipb menu | aipb scenario run <name> | stop | status | overlay on|off");
      return true;
   }
   const char *action = argc >= 3 ? engfuncs.pfnCmd_Argv(2) : "";
   if (std::strcmp(action, "run") == 0 && argc == 4) {
      const bool started = ai::explore::runScenario(engfuncs.pfnCmd_Argv(3));
      if (!server && ent) {
         game.clientPrint(ent, "%s", started ? "AiPB: scenario started." :
            "AiPB: scenario rejected; see server console.");
      }
   }
   else if (std::strcmp(action, "stop") == 0 && argc == 3) {
      ai::explore::stopScenario();
   }
   else if (std::strcmp(action, "status") == 0 && argc == 3) {
      ai::explore::printScenarioStatus(server ? nullptr : ent);
   }
   else if (std::strcmp(action, "overlay") == 0 && argc == 4) {
      const char *value = engfuncs.pfnCmd_Argv(3);
      if (std::strcmp(value, "on") == 0) {
         overlayEnabled = true;
      }
      else if (std::strcmp(value, "off") == 0) {
         overlayEnabled = false;
      }
      else {
         game.print("Usage: aipb scenario overlay on|off");
      }
   }
   else {
      game.print("Usage: aipb scenario run <name> | stop | status | overlay on|off");
   }
   return true;
}

} // namespace

bool handleClient(edict_t *ent) {
   if (!allowed(ent)) {
      return false;
   }
   const char *command = engfuncs.pfnCmd_Argv(0);
   if (std::strcmp(command, "aipb") == 0) {
      return handleCommand(ent, false);
   }
   const int index = game.indexOfPlayer(ent);
   auto &session = players[index].session;
   if (std::strcmp(command, "menuselect") == 0) {
      if (!session.ownsSelection(util.getClient(index).menu != Menu::None)) {
         return false;
      }
      if (engfuncs.pfnCmd_Argc() == 2) {
         const char *arg = engfuncs.pfnCmd_Argv(1);
         if (arg && arg[0] >= '0' && arg[0] <= '9' && arg[1] == '\0') {
            onSelect(ent, arg[0] == '0' ? 10 : arg[0] - '0');
         }
      }
      return true;
   }
   // Another client command may replace the displayed menu.
   session.release();
   return false;
}

void resetPlayer(edict_t *ent) {
   if (!ent) {
      return;
   }
   const int index = game.indexOfPlayer(ent);
   if (index >= 0 && index < kPlayers) {
      players[index] = PlayerMenu {};
   }
}

void resetAll() {
   for (auto &player : players) {
      player = PlayerMenu {};
   }
   nextOverlay = 0.0f;
}

void onExternalMenu(edict_t *ent) {
   if (sendingOwnMenu) {
      return;
   }
   if (ent) {
      if (!game.isPlayerEntity(ent)) {
         return;
      }
      const int index = game.indexOfPlayer(ent);
      if (index >= 0 && index < kPlayers) {
         players[index].session.release();
      }
   }
   else {
      for (auto &player : players) {
         player.session.release();
      }
   }
}

void handleServer() {
   handleCommand(nullptr, true);
}

void frame() {
   if (!overlayEnabled || game.time() < nextOverlay) {
      return;
   }
   nextOverlay = game.time() + 0.5f;
   hudtextparms_t parameters {};
   parameters.channel = 4;
   parameters.x = -1.0f;
   parameters.y = 0.12f;
   parameters.r1 = 240;
   parameters.g1 = 240;
   parameters.b1 = 240;
   parameters.a1 = 255;
   parameters.fadeinTime = 0.0f;
   parameters.fadeoutTime = 0.0f;
   parameters.holdTime = 0.7f;
   const auto label = aipb::explorer::scenario::scenarioMenuOverlayText(
      ai::explore::scenarioRunning(), ai::explore::currentScenarioName());
   for (int i = 0; i < kPlayers; ++i) {
      auto *player = game.playerOfIndex(i);
      if (allowed(player)) {
         game.sendHudMessage(player, parameters, label.c_str());
      }
   }
}

} // namespace ai::menu
