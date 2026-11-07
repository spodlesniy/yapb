//
// AiPB - standalone C++ client for external Model I/O process acceptance.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_model_io_loopback.h>
#include <ai/ai_model_io_external_trial_backend.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv) {
  if (argc != 5) {
    std::fprintf(stderr, "usage: modelio_external_client PORT FRAME (move|none|transport-error|guard-error) TARGET_X\n");
    return 2;
  }
  char *end {};
  errno = 0;
  const unsigned long parsedPort = std::strtoul(argv[1], &end, 10);
  if (errno != 0 || end == argv[1] || *end != '\0' ||
      parsedPort == 0 || parsedPort > 65535) {
    return 2;
  }
  end = nullptr;
  errno = 0;
  const unsigned long parsedFrame = std::strtoul(argv[2], &end, 10);
  if (errno != 0 || end == argv[2] || *end != '\0' ||
      parsedFrame == 0 || parsedFrame > 1000000) {
    return 2;
  }
  const bool expectMove = std::strcmp(argv[3], "move") == 0;
  const bool expectNoDecision = std::strcmp(argv[3], "none") == 0;
  const bool expectTransportError = std::strcmp(argv[3], "transport-error") == 0;
  const bool expectGuardError = std::strcmp(argv[3], "guard-error") == 0;
  if (!expectMove && !expectNoDecision && !expectTransportError && !expectGuardError) {
    return 2;
  }
  end = nullptr;
  errno = 0;
  const float targetX = std::strtof(argv[4], &end);
  if (errno != 0 || end == argv[4] || *end != '\0') {
    return 2;
  }

  ai::runtime_io::ModelIORequest request {};
  request.requestId = parsedFrame;
  request.stamp.sessionId = 7;
  request.stamp.frameId = parsedFrame;
  request.stamp.roundId = 1;
  request.stamp.botEntityIndex = 2;
  request.stamp.gameTime = static_cast<float>(parsedFrame) * 0.02f;
  request.stamp.frameSeconds = 0.02f;
  request.self.origin = { 10.0f, 20.0f, 30.0f };
  request.self.viewAngles = { 0.0f, 0.0f, 0.0f };
  request.self.health = 100.0f;
  request.self.maxSpeed = 250.0f;
  request.self.alive = true;
  request.self.onGround = true;

  ai::runtime_io::ModelIOLoopbackTcpTransport transport(
      static_cast<uint16_t>(parsedPort));
  ai::runtime_io::ModelIOSteadyClock clock {};
  ai::runtime_io::ModelIOTransportBackend backend(transport, clock);
  if (!backend.setTimeoutMs(20)) {
    return 3;
  }
  ai::runtime_io::ModelIOExternalTrialBackend guard(backend);
  if (expectGuardError && !guard.arm({ targetX, 20.0f, 30.0f })) {
    return 3;
  }
  const auto response = expectGuardError ? guard.infer(request) : backend.infer(request);
  const auto &output = response.inference.output;
  if (!ai::runtime_io::modelIOResponseMatches(request, response) ||
      backend.attempts() != 1) {
    std::fprintf(stderr, "missing correlation or attempt\n");
    return 1;
  }
  if (expectTransportError || expectGuardError) {
    const bool transportFailed = backend.lastStatus() !=
        ai::runtime_io::ModelIOTransportStatus::Completed && backend.completed() == 0;
    const bool guarded = backend.lastStatus() ==
        ai::runtime_io::ModelIOTransportStatus::Completed &&
        backend.completed() == 1 && !guard.decided();
    if (response.inference.status != ai::InferenceStatus::Error ||
        (expectTransportError && !transportFailed) ||
        (expectGuardError && !guarded)) {
      std::fprintf(stderr, "unsafe reply accepted status=%u inference=%u\n",
          static_cast<unsigned>(backend.lastStatus()),
          static_cast<unsigned>(response.inference.status));
      return 1;
    }
    std::puts("PASS: unsafe external reply rejected by C++ Model I/O");
    return 0;
  }
  if (backend.lastStatus() != ai::runtime_io::ModelIOTransportStatus::Completed ||
      backend.completed() != 1) {
    std::fprintf(stderr, "transport failed status=%u inference=%u\n",
        static_cast<unsigned>(backend.lastStatus()),
        static_cast<unsigned>(response.inference.status));
    return 1;
  }
  if (expectMove) {
    if (response.inference.status != ai::InferenceStatus::Success ||
        output.actionId != static_cast<uint8_t>(ai::InferenceActionId::MoveToPosition) ||
        output.targetPosition.x != targetX ||
        output.targetPosition.y != 20.0f ||
        output.targetPosition.z != 30.0f ||
        output.confidence != 1.0f) {
      std::fprintf(stderr, "unexpected movement result\n");
      return 1;
    }
  }
  else if (response.inference.status != ai::InferenceStatus::NoDecision ||
           output.actionId != static_cast<uint8_t>(ai::InferenceActionId::None)) {
    std::fprintf(stderr, "unexpected non-NoDecision result\n");
    return 1;
  }
  std::puts("PASS: independent C++ IPC wire exchange");
  return 0;
}
