//
// AiPB - bounded synchronous byte-transport adapter for Model I/O.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstddef>
#include <cstdint>

#include <ai/ai_model_io_wire.h>

namespace ai::runtime_io {

// A transport must enforce its own I/O deadline: this adapter can reject
// late replies but cannot interrupt a callback blocked in an OS operation.
constexpr uint32_t kModelIOTransportDefaultTimeoutMs = 5;
constexpr uint32_t kModelIOTransportMaxTimeoutMs = 20;

enum class ModelIOExchangeStatus : uint8_t {
  Completed,
  Unavailable,
  TimedOut,
  Failed,
};

struct ModelIOExchangeResult {
  ModelIOExchangeStatus status { ModelIOExchangeStatus::Failed };
  size_t responseBytes {};
};

// Caller-owned buffers only. No implicit socket, threads or queued requests.
// Implementations must finish within timeoutMs and respect responseCapacity.
class ModelIOByteTransport {
public:
  virtual ~ModelIOByteTransport() = default;
  virtual ModelIOExchangeResult exchange(
      const uint8_t *request, size_t requestBytes, uint8_t *response,
      size_t responseCapacity, uint32_t timeoutMs) const = 0;
};

class ModelIOMonotonicClock {
public:
  virtual ~ModelIOMonotonicClock() = default;
  virtual uint64_t milliseconds() const = 0;
};

enum class ModelIOTransportStatus : uint8_t {
  Idle,
  Completed,
  InvalidRequest,
  Unavailable,
  TimedOut,
  Failed,
  InvalidClock,
  InvalidResponse,
};

// Explicitly installed per-bot backend; it does not change the default ONNX
// provider, open network connections or start an independent worker.
class ModelIOTransportBackend final : public ModelIOBackend {
private:
  const ModelIOByteTransport &m_transport;
  const ModelIOMonotonicClock &m_clock;
  uint32_t m_timeoutMs { kModelIOTransportDefaultTimeoutMs };
  mutable ModelIOTransportStatus m_lastStatus { ModelIOTransportStatus::Idle };
  mutable uint64_t m_attempts {};
  mutable uint64_t m_completed {};

  ModelIOResponse fail(const ModelIORequest &request, ModelIOTransportStatus status) const {
    m_lastStatus = status;
    ModelIOResponse response {};
    response.requestId = request.requestId;
    response.sessionId = request.stamp.sessionId;
    response.observationFrameId = request.stamp.frameId;
    response.inference.status = InferenceStatus::Error;
    return response;
  }

public:
  ModelIOTransportBackend(const ModelIOByteTransport &transport,
                          const ModelIOMonotonicClock &clock)
      : m_transport(transport), m_clock(clock) {}

  bool setTimeoutMs(uint32_t value) {
    if (value == 0 || value > kModelIOTransportMaxTimeoutMs) {
      return false;
    }
    m_timeoutMs = value;
    return true;
  }

  uint32_t timeoutMs() const { return m_timeoutMs; }
  ModelIOTransportStatus lastStatus() const { return m_lastStatus; }
  uint64_t attempts() const { return m_attempts; }
  uint64_t completed() const { return m_completed; }

  // One synchronous model decision is exactly one bounded byte exchange.
  // Failed attempts never retry or turn partial bytes into a model action.
  ModelIOResponse infer(const ModelIORequest &request) const override {
    uint8_t outgoing[kModelIOWireMaxBytes] {};
    uint8_t incoming[kModelIOWireMaxBytes] {};
    size_t requestBytes {};
    if (!encodeModelIOWireRequest(request, outgoing, sizeof(outgoing), requestBytes)) {
      return fail(request, ModelIOTransportStatus::InvalidRequest);
    }

    const uint64_t started = m_clock.milliseconds();
    ++m_attempts;
    const auto exchanged = m_transport.exchange(
        outgoing, requestBytes, incoming, sizeof(incoming), m_timeoutMs);
    const uint64_t finished = m_clock.milliseconds();

    if (finished < started) {
      return fail(request, ModelIOTransportStatus::InvalidClock);
    }
    if (finished - started > m_timeoutMs ||
        exchanged.status == ModelIOExchangeStatus::TimedOut) {
      return fail(request, ModelIOTransportStatus::TimedOut);
    }

    switch (exchanged.status) {
    case ModelIOExchangeStatus::Unavailable:
      return fail(request, ModelIOTransportStatus::Unavailable);
    case ModelIOExchangeStatus::Failed:
      return fail(request, ModelIOTransportStatus::Failed);
    case ModelIOExchangeStatus::Completed:
      break;
    case ModelIOExchangeStatus::TimedOut:
      return fail(request, ModelIOTransportStatus::TimedOut);
    default:
      return fail(request, ModelIOTransportStatus::Failed);
    }

    if (exchanged.responseBytes == 0 || exchanged.responseBytes > sizeof(incoming)) {
      return fail(request, ModelIOTransportStatus::InvalidResponse);
    }
    ModelIOResponse decoded {};
    if (!decodeModelIOWireResponse(incoming, exchanged.responseBytes, request, decoded)) {
      return fail(request, ModelIOTransportStatus::InvalidResponse);
    }
    m_lastStatus = ModelIOTransportStatus::Completed;
    ++m_completed;
    return decoded;
  }
};

} // namespace ai::runtime_io
