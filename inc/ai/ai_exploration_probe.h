#pragma once

#include <aipb/explorer/engine_bridge.h>

class Bot;

namespace ai::explore {

// Only one explicitly selected bot can be controlled, for a bounded number of frames or milliseconds.
bool begin(int botIndex, const AipbExplorerBridgeCommand &command, int limit, bool milliseconds = false, bool append = false);
bool enqueue(int botIndex, const AipbExplorerBridgeCommand &command, int limit, bool milliseconds);
bool clearQueue();
int queuedCommands();
bool runQueue(const char *savePath);
bool beginReplay(int botIndex);
bool save(const char *filePath);
bool load(const char *filePath);
bool init(int botIndex);
int recordedSteps();
bool report();
bool clear();
bool frame(Bot &bot);
void reset();
void cancelBot(int botIndex);
bool active();

}
