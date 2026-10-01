//
// AiPB - Bot runtime header compile test.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_bot_runtime.h>

static_assert(sizeof(ai::BotRuntime) > 0, "BotRuntime header must remain self-contained");

#include <ai/ai_inference_policy.h>

namespace {
class CompileInferenceProvider final : public ai::InferenceProvider {
public:
  ai::InferenceResult infer(const ai::InferenceInput &) const override {
    return {};
  }
};
} // namespace
