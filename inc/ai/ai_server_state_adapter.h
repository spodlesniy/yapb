//
// AiPB - GoldSrc read-only bot state adapter.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_server_state.h>

class Bot;

namespace ai::runtime_io {

// Called on the game thread from each bot's existing frame callback.
void captureBotFrame(Bot &bot);
void releaseBot(int botSlot);
void resetServerState();

// Copies the most recently captured self state; it is not a perception grant.
bool readServerBotSnapshot(int botSlot, ServerBotSnapshot &snapshot);

} // namespace ai::runtime_io
