//
// AiPB - standalone C++ client for external Model I/O process acceptance.
//
// SPDX-License-Identifier: MIT
//

#include <ai/ai_model_io_loopback.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv) {
  if (argc != 5) {
    std::fprintf(stderr, "usage: modelio_external_client PORT FRAME EXPECTED TARGET_X\n");
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
  if (!expectMove && !expectNoDecision) {
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
  const auto response = backend.infer(request);
  const auto &output = response.inference.output;
  if (!ai::runtime_io::modelIOResponseMatches(request, response) ||
      backend.lastStatus() != ai::runtime_io::ModelIOTransportStatus::Completed ||
      backend.attempts() != 1 || backend.completed() != 1) {
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
