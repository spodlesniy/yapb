//
// AiPB - Bot runtime header compile test.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <type_traits>

#include <ai/ai_bot_runtime.h>
#include <ai/ai_training_recorder.h>

static_assert(sizeof(ai::BotRuntime) > 0, "BotRuntime header must remain self-contained");
static_assert(sizeof(ai::TrainingRecorder) < sizeof(ai::TrainingTransition) * ai::kTrainingTransitionCapacity,
              "BotRuntime training state must not embed the completed transition buffer");

using BotRuntimeRewardProviderSetter = void (ai::BotRuntime::*)(const ai::RewardProvider *);
static_assert(std::is_same_v<decltype(static_cast<BotRuntimeRewardProviderSetter>(&ai::BotRuntime::setRewardProvider)),
                             BotRuntimeRewardProviderSetter>,
              "BotRuntime must expose a reward provider setter");

using BotRuntimeTrainingEpisodeBegin = void (ai::BotRuntime::*)();
static_assert(std::is_same_v<decltype(static_cast<BotRuntimeTrainingEpisodeBegin>(&ai::BotRuntime::beginTrainingEpisode)),
                             BotRuntimeTrainingEpisodeBegin>,
              "BotRuntime must expose a training episode lifecycle entry point");

#include <ai/ai_inference_policy.h>

namespace {
class CompileInferenceProvider final : public ai::InferenceProvider {
public:
  ai::InferenceResult infer(const ai::InferenceInput &) const override {
    return {};
  }
};
} // namespace
