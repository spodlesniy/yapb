//
// AiPB - Bot runtime header compile test.
// AiPB, based on YaPB by YaPB Project Developers <yapb@jeefo.net>, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © Aleksandr Podlesnyi <spodlesniy@gmail.com>.
//
// SPDX-License-Identifier: MIT
//

#include <type_traits>

#include <yapb.h>

#include <ai/ai_bot_runtime.h>
#include <ai/ai_training_recorder.h>

static_assert(sizeof(ai::BotRuntime) > 0, "BotRuntime header must remain self-contained");
static_assert(sizeof(ai::TrainingRecorder) < sizeof(ai::TrainingTransition) * ai::kTrainingTransitionCapacity,
              "BotRuntime training state must not embed the completed transition buffer");

using BotRuntimeRewardProviderSetter = void (ai::BotRuntime::*)(const ai::RewardProvider *);
static_assert(std::is_same_v<decltype(static_cast<BotRuntimeRewardProviderSetter>(&ai::BotRuntime::setRewardProvider)),
                             BotRuntimeRewardProviderSetter>,
              "BotRuntime must expose a reward provider setter");

using BotRuntimeModeSetter = void (ai::BotRuntime::*)(ai::ControlMode, const ai::Observation &);
static_assert(std::is_same_v<decltype(static_cast<BotRuntimeModeSetter>(&ai::BotRuntime::setMode)), BotRuntimeModeSetter>,
              "BotRuntime mode setter must accept the current observation");

using BotRuntimeTrainingEpisodeBegin = void (ai::BotRuntime::*)();
static_assert(std::is_same_v<decltype(static_cast<BotRuntimeTrainingEpisodeBegin>(&ai::BotRuntime::beginTrainingEpisode)),
                             BotRuntimeTrainingEpisodeBegin>,
              "BotRuntime must expose a training episode lifecycle entry point");

using BotRuntimeTrainingBufferGetter = ai::TrainingBuffer &(ai::BotRuntime::*)();
static_assert(std::is_same_v<decltype(static_cast<BotRuntimeTrainingBufferGetter>(&ai::BotRuntime::trainingBuffer)),
                             BotRuntimeTrainingBufferGetter>,
              "BotRuntime must expose a training buffer accessor");

using BotTrainingBufferGetter = ai::TrainingBuffer &(Bot::*)();
static_assert(std::is_same_v<decltype(static_cast<BotTrainingBufferGetter>(&Bot::getAITrainingBuffer)),
                             BotTrainingBufferGetter>,
              "Bot must expose a training buffer accessor");

#include <ai/ai_inference_policy.h>

namespace {
class CompileInferenceProvider final : public ai::InferenceProvider {
public:
  ai::InferenceResult infer(const ai::InferenceInput &) const override {
    return {};
  }
};
} // namespace
