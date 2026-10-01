//
// AiPB - Bot runtime header compile test.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_bot_runtime.h>

static_assert (sizeof (ai::BotRuntime) > 0, "BotRuntime header must remain self-contained");
