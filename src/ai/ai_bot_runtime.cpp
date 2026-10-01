//
// AiPB - Bot AI runtime.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_bot_runtime.h>

namespace ai {

BotRuntime::BotRuntime (Bot &bot)
   : m_executor (bot),
     m_pipeline (m_executor, m_actionState),
     m_loop (m_controller, m_pipeline) {
}

} // namespace ai
