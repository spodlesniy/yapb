//
// AiPB - local TCP loopback Model I/O transport tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <cstddef>
#include <cstdint>

#include <ai/ai_model_io_loopback.h>

#ifndef _WIN32
#include <chrono>
#include <cstring>
#include <thread>
#endif

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

io::ModelIORequest requestFixture() {
  io::ModelIORequest request {};
  request.requestId = 15;
  request.stamp.sessionId = 3;
  request.stamp.frameId = 12;
  request.stamp.roundId = 1;
  request.stamp.botEntityIndex = 2;
  request.stamp.gameTime = 2.0f;
  request.stamp.frameSeconds = 0.02f;
  request.self.alive = true;
  request.self.onGround = true;
  request.self.health = 100.0f;
  request.self.maxSpeed = 250.0f;
  return request;
}

#ifndef _WIN32

class LocalServer final {
private:
  int m_socket { -1 };
  uint16_t m_port {};

public:
  LocalServer() {
    m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket < 0) {
      return;
    }
    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(m_socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0 ||
        ::listen(m_socket, 1) != 0) {
      ::close(m_socket);
      m_socket = -1;
      return;
    }
    socklen_t len = sizeof(address);
    if (::getsockname(m_socket, reinterpret_cast<sockaddr *>(&address), &len) != 0) {
      ::close(m_socket);
      m_socket = -1;
      return;
    }
    m_port = ntohs(address.sin_port);
  }

  ~LocalServer() {
    if (m_socket >= 0) {
      ::close(m_socket);
    }
  }

  LocalServer(const LocalServer &) = delete;
  LocalServer &operator=(const LocalServer &) = delete;

  bool valid() const { return m_socket >= 0 && m_port != 0; }
  uint16_t port() const { return m_port; }

  // The test-side accept has a finite timeout so a failed client cannot hang CI.
  int acceptOne() const {
    fd_set readable {};
    FD_ZERO(&readable);
    FD_SET(m_socket, &readable);
    timeval timeout {};
    timeout.tv_usec = 250000;
    if (select(m_socket + 1, &readable, nullptr, nullptr, &timeout) <= 0) {
      return -1;
    }
    return ::accept(m_socket, nullptr, nullptr);
  }
};

bool sendFrame(int client, const uint8_t *buffer, size_t count, bool fragmented) {
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags = MSG_NOSIGNAL;
#endif
  if (fragmented && count > 12) {
    const int part = ::send(client, reinterpret_cast<const char *>(buffer), 12, flags);
    if (part != 12) {
      return false;
    }
    buffer += 12;
    count -= 12;
  }
  while (count > 0) {
    const int sent = ::send(client, reinterpret_cast<const char *>(buffer),
                            static_cast<int>(count), flags);
    if (sent <= 0) {
      return false;
    }
    buffer += sent;
    count -= static_cast<size_t>(sent);
  }
  return true;
}

bool receiveRequest(int client, io::ModelIORequest &decoded) {
  uint8_t buffer[io::kModelIOWireMaxBytes] {};
  size_t size {};
  // Request carries its authoritative length in the 12-byte wire header.
  while (size < io::kModelIOWireHeaderBytes) {
    const int got = ::recv(client, reinterpret_cast<char *>(buffer + size),
                           static_cast<int>(io::kModelIOWireHeaderBytes - size), 0);
    if (got <= 0) {
      return false;
    }
    size += static_cast<size_t>(got);
  }
  const uint32_t payload = static_cast<uint32_t>(buffer[8]) |
      (static_cast<uint32_t>(buffer[9]) << 8) |
      (static_cast<uint32_t>(buffer[10]) << 16) |
      (static_cast<uint32_t>(buffer[11]) << 24);
  if (payload == 0 || payload > sizeof(buffer) - io::kModelIOWireHeaderBytes) {
    return false;
  }
  const size_t total = io::kModelIOWireHeaderBytes + payload;
  while (size < total) {
    const int got = ::recv(client, reinterpret_cast<char *>(buffer + size),
                           static_cast<int>(total - size), 0);
    if (got <= 0) {
      return false;
    }
    size += static_cast<size_t>(got);
  }
  return io::decodeModelIOWireRequest(buffer, total, decoded);
}

enum class ServerMode { Valid, Oversized, Truncated, Silent };

void serveOnce(const LocalServer &server, ServerMode mode, bool fragmented) {
  const int client = server.acceptOne();
  if (client < 0) {
    return;
  }
#ifdef SO_NOSIGPIPE
  int noSigPipe = 1;
  setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe));
#endif
  if (mode == ServerMode::Silent) {
    std::this_thread::sleep_for(std::chrono::milliseconds(35));
    ::close(client);
    return;
  }
  io::ModelIORequest request {};
  if (receiveRequest(client, request)) {
    io::ModelIOResponse response {};
    response.requestId = request.requestId;
    response.sessionId = request.stamp.sessionId;
    response.observationFrameId = request.stamp.frameId;
    response.inference.status = ai::InferenceStatus::Success;
    response.inference.output.actionId =
        static_cast<uint8_t>(ai::InferenceActionId::MoveToPosition);
    response.inference.output.targetPosition = { 96.0f, 0.0f, 32.0f };
    response.inference.output.confidence = 1.0f;
    uint8_t bytes[io::kModelIOWireMaxBytes] {};
    size_t written {};
    if (io::encodeModelIOWireResponse(response, bytes, sizeof(bytes), written)) {
      if (mode == ServerMode::Oversized) {
        bytes[8] = 0xff;
        bytes[9] = 0xff;
        bytes[10] = 0xff;
        bytes[11] = 0xff;
        sendFrame(client, bytes, io::kModelIOWireHeaderBytes, false);
      }
      else {
        if (mode == ServerMode::Truncated) {
          --written;
        }
        sendFrame(client, bytes, written, fragmented);
      }
    }
  }
  ::close(client);
}
#endif

} // namespace

AI_TEST(testModelIOLoopbackRejectsInvalidEndpointAndBuffers) {
  io::ModelIOLoopbackTcpTransport disabled(0);
  const uint8_t request[io::kModelIOWireHeaderBytes] {};
  uint8_t reply[io::kModelIOWireMaxBytes] {};
  const auto result = disabled.exchange(request, sizeof(request), reply, sizeof(reply), 5);
  expect(result.status == io::ModelIOExchangeStatus::Failed &&
         result.responseBytes == 0, "port zero is disabled without opening a socket");
  io::ModelIOLoopbackTcpTransport transport(1234);
  expect(transport.port() == 1234, "only a loopback TCP port is configurable");
  expect(transport.exchange(nullptr, sizeof(request), reply, sizeof(reply), 5).status ==
         io::ModelIOExchangeStatus::Failed, "null request is refused");
  expect(transport.exchange(request, sizeof(request), nullptr, sizeof(reply), 5).status ==
         io::ModelIOExchangeStatus::Failed, "null output buffer is refused");
  expect(transport.exchange(request, sizeof(request), reply, sizeof(reply), 0).status ==
         io::ModelIOExchangeStatus::Failed, "no unbounded timeout is allowed");
  expect(transport.exchange(request, sizeof(request), reply, sizeof(reply),
         io::kModelIOTransportMaxTimeoutMs + 1).status ==
         io::ModelIOExchangeStatus::Failed, "timeout cannot exceed configured maximum");
  io::ModelIOSteadyClock clock {};
  expect(clock.milliseconds() > 0, "platform steady clock exposes a monotonic count");
}

#ifndef _WIN32

AI_TEST(testModelIOLoopbackCompleteAndFragmentedFrames) {
  for (int fragmented = 0; fragmented < 2; ++fragmented) {
    LocalServer server {};
    expect(server.valid(), "ephemeral IPv4 loopback listener available");
    if (!server.valid()) {
      continue;
    }
    std::thread worker([&server, fragmented]() {
      serveOnce(server, ServerMode::Valid, fragmented != 0);
    });
    const auto request = requestFixture();
    uint8_t outgoing[io::kModelIOWireMaxBytes] {};
    uint8_t incoming[io::kModelIOWireMaxBytes] {};
    size_t outgoingSize {};
    expect(io::encodeModelIOWireRequest(request, outgoing, sizeof(outgoing), outgoingSize),
           "request fixture encodes to valid wire frame");
    io::ModelIOLoopbackTcpTransport transport(server.port());
    const auto result = transport.exchange(outgoing, outgoingSize,
                                           incoming, sizeof(incoming), 20);
    worker.join();
    io::ModelIOResponse decoded {};
    expect(result.status == io::ModelIOExchangeStatus::Completed &&
           io::decodeModelIOWireResponse(incoming, result.responseBytes, request, decoded) &&
           decoded.inference.output.targetPosition.x == 96.0f,
           "real local TCP roundtrip reconstructs bounded Model I/O response");
  }
}

AI_TEST(testModelIOLoopbackRejectsOversizedAndTruncatedFrames) {
  const ServerMode modes[] { ServerMode::Oversized, ServerMode::Truncated };
  for (const auto mode : modes) {
    LocalServer server {};
    expect(server.valid(), "listener for malformed response");
    if (!server.valid()) {
      continue;
    }
    std::thread worker([&server, mode]() { serveOnce(server, mode, false); });
    const auto request = requestFixture();
    uint8_t outgoing[io::kModelIOWireMaxBytes] {};
    uint8_t incoming[io::kModelIOWireMaxBytes] {};
    size_t outgoingSize {};
    expect(io::encodeModelIOWireRequest(request, outgoing, sizeof(outgoing), outgoingSize),
           "test request encodes");
    io::ModelIOLoopbackTcpTransport transport(server.port());
    const auto result = transport.exchange(outgoing, outgoingSize,
                                           incoming, sizeof(incoming), 20);
    worker.join();
    expect(result.status != io::ModelIOExchangeStatus::Completed &&
           result.responseBytes == 0, "oversized or truncated frame is rejected");
  }
}

AI_TEST(testModelIOLoopbackMissingEndpointAndDeadline) {
  const auto request = requestFixture();
  uint8_t outgoing[io::kModelIOWireMaxBytes] {};
  uint8_t incoming[io::kModelIOWireMaxBytes] {};
  size_t outgoingSize {};
  expect(io::encodeModelIOWireRequest(request, outgoing, sizeof(outgoing), outgoingSize),
         "deadline test fixture encodes");

  LocalServer unavailable {};
  expect(unavailable.valid(), "reserved local port created");
  if (!unavailable.valid()) {
    return;
  }
  // A listening socket with no accept is not used to test refusal. A silent
  // peer instead provides deterministic proof of an enforced receive deadline.
  std::thread worker([&unavailable]() {
    serveOnce(unavailable, ServerMode::Silent, false);
  });
  io::ModelIOLoopbackTcpTransport transport(unavailable.port());
  const auto result = transport.exchange(outgoing, outgoingSize,
                                         incoming, sizeof(incoming), 10);
  worker.join();
  expect(result.status == io::ModelIOExchangeStatus::TimedOut &&
         result.responseBytes == 0, "silent peer cannot hold the game thread past deadline");
}

#endif
