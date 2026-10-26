#pragma once

#include <ai/ai_navvis_format.h>

namespace ai::navvis {
void frame (edict_t *editor);
void toggle (edict_t *editor, int option);
void radius (float value);
float radius ();
bool canView (edict_t *editor);
void resetPlayer (edict_t *editor);
void reset ();

} // namespace ai::navvis
