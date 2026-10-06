//
// AiPB - Bot-to-AI observation adapter.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <ai/ai_observation_builder.h>

class Bot;

namespace ai {

// Reports whether the cached YaPB game state contains a dropped C4 backpack.
bool hasDroppedBombObjective();

// Builds an engine-independent snapshot from the live YaPB bot state.
// The adapter only observes state; it does not change bot behavior.
ObservationInput buildObservationInput(const Bot &bot);

} // namespace ai
