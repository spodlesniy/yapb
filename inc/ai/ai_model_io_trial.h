//
// AiPB - opt-in deterministic Model I/O server integration trial.
//
// SPDX-License-Identifier: MIT
//

#pragma once

class Bot;

namespace ai::model_io_trial {

bool handleServer();
bool frame(Bot &bot);
void reset();
void cancelBot(int botIndex);
bool active();

} // namespace ai::model_io_trial
