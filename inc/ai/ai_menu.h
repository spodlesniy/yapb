#pragma once


namespace ai::menu {

// Called before YaPB's own client-command and menu handlers.
bool handleClient(edict_t *player);
void handleServer();
void resetPlayer(edict_t *player);
void resetAll();
void onExternalMenu(edict_t *player);
void frame();

} // namespace ai::menu
