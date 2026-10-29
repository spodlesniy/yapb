#pragma once


namespace ai::menu {

// Called before YaPB's own client-command and menu handlers.
bool handleClient(edict_t *player);
void handleServer();
void frame();

} // namespace ai::menu
