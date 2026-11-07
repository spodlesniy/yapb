//
// AiPB - bounded synchronous Model I/O byte transport tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <cstddef>
#include <cstdint>

#include <ai/ai_inference_policy.h>
#include <ai/ai_model_io_transport.h>

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

io::ModelIORequest transportRequest() {
  io::ModelIORequest request {};
  request.requestId = 9;
  request.stamp.sessionId = 3;
  request.stamp.frameId = 12;
  request.stamp.roundId = 1;
  request.stamp.botEntityIndex = 2;
  request.stamp.gameTime = 2.0f;
  request.stamp.frameSeconds = 0.02f;
  request.self.origin = { 10.0f, 20.0f, 30.0f };
  request.self.alive = true;
  request.self.onGround = true;
  request.self.health = 100.0f;
  request.self.maxSpeed = 250.0f;
  return request;
}

class TestClock final : public io::ModelIOMonotonicClock {
public:
  uint64_t time { 100 };
  uint64_t milliseconds() const override { return time; }
};

class TestDecisionBackend final : public io::ModelIOBackend {
public:
  mutable unsigned calls {};
  mutable io::ModelIORequest last {};
  ai::InferenceStatus replyStatus { ai::InferenceStatus::Success };

  io::ModelIOResponse infer(const io::ModelIORequest &request) const override {
    ++calls;
    last = request;
    io::ModelIOResponse response {};
    response.requestId = request.requestId;
    response.sessionId = request.stamp.sessionId;
    response.observationFrameId = request.stamp.frameId;
    response.inference.status = replyStatus;
    response.inference.output.actionId =
        static_cast<uint8_t>(ai::InferenceActionId::MoveToPosition);
    response.inference.output.targetPosition = { 96.0f, 0.0f, 32.0f };
    response.inference.output.confidence = 1.0f;
    return response;
  }
};

class TestByteTransport final : public io::ModelIOByteTransport {
public:
  enum class Mode {
    Valid,
    Unavailable,
    TimedOut,
    Failed,
    Late,
    ClockRegression,
    Corrupt,
    Truncated,
    ForeignRequest,
    Oversized,
  };
  TestDecisionBackend &model;
  TestClock &clock;
  mutable unsigned calls {};
  mutable uint32_t budgetSeen {};
  Mode mode { Mode::Valid };

  TestByteTransport(TestDecisionBackend &backend, TestClock &source)
      : model(backend), clock(source) {}

  io::ModelIOExchangeResult exchange(
      const uint8_t *request, size_t requestBytes, uint8_t *response,
      size_t responseCapacity, uint32_t timeoutMs) const override {
    ++calls;
    budgetSeen = timeoutMs;
    if (mode == Mode::Unavailable) {
      return { io::ModelIOExchangeStatus::Unavailable, 0 };
    }
    if (mode == Mode::TimedOut) {
      return { io::ModelIOExchangeStatus::TimedOut, 0 };
    }
    if (mode == Mode::Failed) {
      return { io::ModelIOExchangeStatus::Failed, 0 };
    }

    io::ModelIORequest decoded {};
    if (!io::decodeModelIOWireRequest(request, requestBytes, decoded)) {
      return { io::ModelIOExchangeStatus::Failed, 0 };
    }
    auto result = model.infer(decoded);
    if (mode == Mode::ForeignRequest) {
      ++result.requestId;
    }
    size_t written {};
    if (!io::encodeModelIOWireResponse(result, response, responseCapacity, written)) {
      return { io::ModelIOExchangeStatus::Failed, 0 };
    }
    if (mode == Mode::Corrupt) {
      response[0] = 0;
    }
    if (mode == Mode::Late) {
      clock.time += timeoutMs + 1;
    }
    if (mode == Mode::ClockRegression) {
      --clock.time;
    }
    if (mode == Mode::Oversized) {
      return { io::ModelIOExchangeStatus::Completed, responseCapacity + 1 };
    }
    if (mode == Mode::Truncated) {
      --written;
    }
    return { io::ModelIOExchangeStatus::Completed, written };
  }
};

ai::Observation transportObservation(float time) {
  ai::Observation observation {};
  observation.gameTime = time;
  observation.bot.alive = true;
  observation.bot.health = 100.0f;
  return observation;
}

} // namespace

AI_TEST(testModelIOTransportRoundTripAndBudget) {
  TestClock clock {};
  TestDecisionBackend model {};
  TestByteTransport transport(model, clock);
  io::ModelIOTransportBackend backend(transport, clock);
  expect(backend.timeoutMs() == io::kModelIOTransportDefaultTimeoutMs,
         "transport uses a bounded default budget");
  expect(!backend.setTimeoutMs(0) &&
         !backend.setTimeoutMs(io::kModelIOTransportMaxTimeoutMs + 1) &&
         backend.timeoutMs() == io::kModelIOTransportDefaultTimeoutMs,
         "invalid budget never silently replaces old budget");
  expect(backend.setTimeoutMs(10), "valid budget accepted");

  const auto input = transportRequest();
  const auto reply = backend.infer(input);
  expect(reply.inference.status == ai::InferenceStatus::Success &&
         io::modelIOResponseMatches(input, reply) &&
         reply.inference.output.targetPosition.x == 96.0f,
         "wire response carries a correlated real backend decision");
  expect(model.calls == 1 && transport.calls == 1 &&
         transport.budgetSeen == 10 && model.last.requestId == input.requestId,
         "exactly one exchange and one model request with configured time budget");
  expect(backend.lastStatus() == io::ModelIOTransportStatus::Completed &&
         backend.attempts() == 1 && backend.completed() == 1,
         "completed exchange counted separately from attempted work");

  model.replyStatus = ai::InferenceStatus::NoDecision;
  const auto empty = backend.infer(input);
  expect(empty.inference.status == ai::InferenceStatus::NoDecision &&
         backend.lastStatus() == io::ModelIOTransportStatus::Completed &&
         backend.attempts() == 2 && backend.completed() == 2,
         "valid NoDecision is not a network error or fabricated action");
}

AI_TEST(testModelIOTransportRejectsBadRequestsWithoutIO) {
  TestClock clock {};
  TestDecisionBackend model {};
  TestByteTransport transport(model, clock);
  io::ModelIOTransportBackend backend(transport, clock);

  auto invalid = transportRequest();
  invalid.requestId = 0;
  expect(backend.infer(invalid).inference.status == ai::InferenceStatus::Error &&
         backend.lastStatus() == io::ModelIOTransportStatus::InvalidRequest,
         "invalid request ID fails before exchange");
  invalid = transportRequest();
  invalid.visibleEnemy.position.x = 123.0f;
  expect(backend.infer(invalid).inference.status == ai::InferenceStatus::Error &&
         backend.lastStatus() == io::ModelIOTransportStatus::InvalidRequest,
         "hidden coordinates fail before wire I/O");
  invalid = transportRequest();
  invalid.features.schemaVersion = 0;
  expect(backend.infer(invalid).inference.status == ai::InferenceStatus::Error &&
         backend.lastStatus() == io::ModelIOTransportStatus::InvalidRequest,
         "unsupported feature schema cannot be sent");
  expect(backend.attempts() == 0 && backend.completed() == 0 &&
         transport.calls == 0 && model.calls == 0,
         "invalid inputs never reach a transport or model");
}

AI_TEST(testModelIOTransportFailureModesAndRecovery) {
  TestClock clock {};
  TestDecisionBackend model {};
  TestByteTransport transport(model, clock);
  io::ModelIOTransportBackend backend(transport, clock);
  const auto input = transportRequest();

  struct FailureCase {
    TestByteTransport::Mode mode;
    io::ModelIOTransportStatus status;
  };
  const FailureCase cases[] {
    { TestByteTransport::Mode::Unavailable, io::ModelIOTransportStatus::Unavailable },
    { TestByteTransport::Mode::TimedOut, io::ModelIOTransportStatus::TimedOut },
    { TestByteTransport::Mode::Failed, io::ModelIOTransportStatus::Failed },
    { TestByteTransport::Mode::Late, io::ModelIOTransportStatus::TimedOut },
    { TestByteTransport::Mode::Corrupt, io::ModelIOTransportStatus::InvalidResponse },
    { TestByteTransport::Mode::Truncated, io::ModelIOTransportStatus::InvalidResponse },
    { TestByteTransport::Mode::ForeignRequest, io::ModelIOTransportStatus::InvalidResponse },
    { TestByteTransport::Mode::Oversized, io::ModelIOTransportStatus::InvalidResponse },
    { TestByteTransport::Mode::ClockRegression, io::ModelIOTransportStatus::InvalidClock },
  };

  for (const auto &failure : cases) {
    transport.mode = failure.mode;
    const auto result = backend.infer(input);
    expect(result.inference.status == ai::InferenceStatus::Error &&
           backend.lastStatus() == failure.status,
           "transport errors and invalid remote responses fail closed");
    expect(io::modelIOResponseMatches(input, result),
           "failed exchange preserves request/session/frame envelope");
    clock.time = 100;
  }
  expect(backend.attempts() == 9 && backend.completed() == 0 &&
         transport.calls == 9, "no failed exchange is counted as completed");

  transport.mode = TestByteTransport::Mode::Valid;
  const auto restored = backend.infer(input);
  expect(restored.inference.status == ai::InferenceStatus::Success &&
         backend.lastStatus() == io::ModelIOTransportStatus::Completed &&
         backend.attempts() == 10 && backend.completed() == 1,
         "next explicitly issued request may succeed without stale reply reuse");
}

AI_TEST(testModelIOTransportThroughExistingInferencePolicy) {
  TestClock clock {};
  TestDecisionBackend model {};
  TestByteTransport transport(model, clock);
  io::ModelIOTransportBackend backend(transport, clock);
  io::ModelIOAdapter adapter {};
  adapter.setBackend(&backend);
  ai::InferencePolicy policy { &adapter };

  const auto raw = transportRequest();
  io::ServerBotSnapshot self {};
  self.stamp = raw.stamp;
  self.self = raw.self;
  io::PerceptionFrame perception {};
  perception.self = self;
  io::ControlFrame control {};
  control.stamp = self.stamp;
  control.owner = io::ControlOwner::Neural;

  expect(adapter.bindFrame(self, perception, control),
         "current permitted frame binds existing Model I/O adapter");
  expect(policy.decide(transportObservation(raw.stamp.gameTime)).type ==
         ai::ActionType::MoveToPosition,
         "existing InferencePolicy decodes a decision from wire backend");
  expect(backend.completed() == 1 && model.calls == 1,
         "policy path performs one complete wire exchange");
  expect(policy.decide(transportObservation(raw.stamp.gameTime)).type ==
         ai::ActionType::None && model.calls == 1,
         "same source frame cannot repeat an external model request");

  auto next = self;
  ++next.stamp.frameId;
  next.stamp.gameTime += 0.02f;
  perception.self = next;
  control.stamp = next.stamp;
  transport.mode = TestByteTransport::Mode::TimedOut;
  expect(adapter.bindFrame(next, perception, control),
         "next frame binds after a completed model action");
  expect(policy.decide(transportObservation(next.stamp.gameTime)).type ==
         ai::ActionType::None &&
         backend.lastStatus() == io::ModelIOTransportStatus::TimedOut,
         "timed-out external decision emits no semantic action");
}
