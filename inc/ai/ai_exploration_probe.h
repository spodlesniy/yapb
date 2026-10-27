#pragma once

#include <aipb/explorer/engine_bridge.h>

class Bot;

namespace ai::explore {

// Only one explicitly selected bot can be controlled, for a bounded number of frames or milliseconds.
bool begin(int botIndex, const AipbExplorerBridgeCommand &command, int limit, bool milliseconds = false);
bool frame(Bot &bot);
void reset();
void cancelBot(int botIndex);
bool active();

}
