#pragma once

#include <aipb/explorer/engine_bridge.h>

class Bot;

namespace ai::explore {

// Only one explicitly selected bot can be controlled, for at most 128 engine frames.
bool begin(int botIndex, const AipbExplorerBridgeCommand &command, int frameCount);
bool frame(Bot &bot);
void reset();
void cancelBot(int botIndex);
bool active();

}
