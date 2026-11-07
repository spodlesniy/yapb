//
// AiPB - bounded canonical wire format for external Model I/O backends.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include <ai/ai_model_io.h>

namespace ai::runtime_io {

constexpr uint16_t kModelIOWireVersion = 1;
constexpr size_t kModelIOWireMaxBytes = 2048;
constexpr size_t kModelIOWireHeaderBytes = 12;

enum class ModelIOWireKind : uint16_t {
  Request = 1,
  Response = 2,
};

namespace model_io_wire_detail {

static_assert(sizeof(float) == sizeof(uint32_t) && std::numeric_limits<float>::is_iec559,
              "Model I/O wire v1 requires IEEE-754 binary32");

constexpr uint32_t kMagic = 0x42504941u; // "AIPB" in little-endian bytes.

class Writer final {
private:
  uint8_t *m_data {};
  size_t m_capacity {};
  size_t m_offset {};

public:
  Writer(uint8_t *data, size_t capacity) : m_data(data), m_capacity(capacity) {}

  size_t size() const { return m_offset; }

  bool u8(uint8_t value) {
    if (m_data == nullptr || m_offset >= m_capacity) {
      return false;
    }
    m_data[m_offset++] = value;
    return true;
  }

  bool u16(uint16_t value) {
    return u8(static_cast<uint8_t>(value)) && u8(static_cast<uint8_t>(value >> 8));
  }

  bool u32(uint32_t value) {
    for (int i = 0; i < 4; ++i) {
      if (!u8(static_cast<uint8_t>(value >> (i * 8)))) {
        return false;
      }
    }
    return true;
  }

  bool u64(uint64_t value) {
    for (int i = 0; i < 8; ++i) {
      if (!u8(static_cast<uint8_t>(value >> (i * 8)))) {
        return false;
      }
    }
    return true;
  }

  bool i32(int32_t value) {
    uint32_t bits {};
    std::memcpy(&bits, &value, sizeof(bits));
    return u32(bits);
  }

  bool f32(float value) {
    if (!std::isfinite(value)) {
      return false;
    }
    uint32_t bits {};
    std::memcpy(&bits, &value, sizeof(bits));
    return u32(bits);
  }

  bool vec3(const Vec3 &value) {
    return f32(value.x) && f32(value.y) && f32(value.z);
  }

  bool finish(size_t &written) {
    if (m_data == nullptr || m_offset < kModelIOWireHeaderBytes ||
        m_offset > kModelIOWireMaxBytes) {
      return false;
    }
    const auto payload = static_cast<uint32_t>(m_offset - kModelIOWireHeaderBytes);
    for (int i = 0; i < 4; ++i) {
      m_data[8 + i] = static_cast<uint8_t>(payload >> (i * 8));
    }
    written = m_offset;
    return true;
  }
};

class Reader final {
private:
  const uint8_t *m_data {};
  size_t m_size {};
  size_t m_offset {};

public:
  Reader(const uint8_t *data, size_t size) : m_data(data), m_size(size) {}

  bool done() const { return m_offset == m_size; }

  bool u8(uint8_t &value) {
    if (m_data == nullptr || m_offset >= m_size) {
      return false;
    }
    value = m_data[m_offset++];
    return true;
  }

  bool u16(uint16_t &value) {
    uint8_t low {}, high {};
    if (!u8(low) || !u8(high)) {
      return false;
    }
    value = static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
    return true;
  }

  bool u32(uint32_t &value) {
    value = 0;
    for (int i = 0; i < 4; ++i) {
      uint8_t byte {};
      if (!u8(byte)) {
        return false;
      }
      value |= static_cast<uint32_t>(byte) << (i * 8);
    }
    return true;
  }

  bool u64(uint64_t &value) {
    value = 0;
    for (int i = 0; i < 8; ++i) {
      uint8_t byte {};
      if (!u8(byte)) {
        return false;
      }
      value |= static_cast<uint64_t>(byte) << (i * 8);
    }
    return true;
  }

  bool i32(int32_t &value) {
    uint32_t bits {};
    if (!u32(bits)) {
      return false;
    }
    std::memcpy(&value, &bits, sizeof(bits));
    return true;
  }

  bool f32(float &value) {
    uint32_t bits {};
    if (!u32(bits)) {
      return false;
    }
    std::memcpy(&value, &bits, sizeof(bits));
    return std::isfinite(value);
  }

  bool vec3(Vec3 &value) {
    return f32(value.x) && f32(value.y) && f32(value.z);
  }

  bool header(ModelIOWireKind kind) {
    uint32_t magic {}, payload {};
    uint16_t version {}, actualKind {};
    return m_size >= kModelIOWireHeaderBytes && m_size <= kModelIOWireMaxBytes &&
           u32(magic) && u16(version) && u16(actualKind) && u32(payload) &&
           magic == kMagic && version == kModelIOWireVersion &&
           actualKind == static_cast<uint16_t>(kind) &&
           payload == m_size - kModelIOWireHeaderBytes;
  }
};

inline bool writeHeader(Writer &writer, ModelIOWireKind kind) {
  return writer.u32(kMagic) && writer.u16(kModelIOWireVersion) &&
         writer.u16(static_cast<uint16_t>(kind)) && writer.u32(0);
}

inline bool writeStamp(Writer &writer, const FrameStamp &stamp) {
  return writer.u32(stamp.schemaVersion) && writer.u64(stamp.sessionId) &&
         writer.u64(stamp.frameId) && writer.u64(stamp.roundId) &&
         writer.f32(stamp.gameTime) && writer.f32(stamp.frameSeconds) &&
         writer.i32(stamp.botEntityIndex);
}

inline bool readStamp(Reader &reader, FrameStamp &stamp) {
  return reader.u32(stamp.schemaVersion) && reader.u64(stamp.sessionId) &&
         reader.u64(stamp.frameId) && reader.u64(stamp.roundId) &&
         reader.f32(stamp.gameTime) && reader.f32(stamp.frameSeconds) &&
         reader.i32(stamp.botEntityIndex);
}

inline bool writeSelf(Writer &writer, const BotSelfState &self) {
  return writer.vec3(self.origin) && writer.vec3(self.velocity) &&
         writer.vec3(self.viewAngles) && writer.f32(self.health) &&
         writer.f32(self.armor) && writer.f32(self.maxSpeed) &&
         writer.u8(self.alive ? 1 : 0) && writer.u8(self.onGround ? 1 : 0) &&
         writer.u8(self.crouching ? 1 : 0);
}

inline bool readBoolean(Reader &reader, bool &value) {
  uint8_t raw {};
  if (!reader.u8(raw) || raw > 1) {
    return false;
  }
  value = raw != 0;
  return true;
}

inline bool readSelf(Reader &reader, BotSelfState &self) {
  return reader.vec3(self.origin) && reader.vec3(self.velocity) &&
         reader.vec3(self.viewAngles) && reader.f32(self.health) &&
         reader.f32(self.armor) && reader.f32(self.maxSpeed) &&
         readBoolean(reader, self.alive) && readBoolean(reader, self.onGround) &&
         readBoolean(reader, self.crouching);
}

inline bool validRequest(const ModelIORequest &request) {
  if (request.schemaVersion != kModelIOSchemaVersion || request.requestId == 0 ||
      validateFrame(request.stamp) != FrameValidation::Valid ||
      !validSelfState(request.self) ||
      !request.features.hasSupportedSchema() ||
      request.eventCount > kPerceptionEventsPerFrame) {
    return false;
  }
  PerceptionFrame perception {};
  perception.visibleEnemy = request.visibleEnemy;
  perception.eventCount = request.eventCount;
  for (uint8_t i = 0; i < request.eventCount; ++i) {
    perception.events[i] = request.events[i];
  }
  if (!validModelIOPerception(perception, request.stamp)) {
    return false;
  }
  if (request.hasFeedback) {
    const auto &feedback = request.feedback;
    if (feedback.sessionId != request.stamp.sessionId || feedback.decisionFrameId == 0 ||
        feedback.decisionFrameId > feedback.observedFrameId ||
        feedback.observedFrameId > request.stamp.frameId ||
        feedback.result.action == ActionType::None ||
        (feedback.result.type != ActionResultType::Accepted && !feedback.result.isTerminal()) ||
        !std::isfinite(feedback.result.elapsedTime) || feedback.result.elapsedTime < 0.0f) {
      return false;
    }
  }
  for (size_t i = 0; i < kInferenceFeatureCount; ++i) {
    if (!std::isfinite(request.features.values[i])) {
      return false;
    }
  }
  return true;
}

inline bool validResponse(const ModelIOResponse &response) {
  const auto &output = response.inference.output;
  return response.schemaVersion == kModelIOSchemaVersion &&
         response.requestId != 0 && response.sessionId != 0 &&
         response.observationFrameId != 0 &&
         static_cast<uint8_t>(response.inference.status) <=
             static_cast<uint8_t>(InferenceStatus::Error) &&
         output.hasSupportedSchema() &&
         output.actionId < static_cast<uint8_t>(InferenceActionId::Count) &&
         finiteVector(output.targetPosition) &&
         std::isfinite(output.duration) && std::isfinite(output.confidence);
}

} // namespace model_io_wire_detail

// One complete message is always encoded into caller-owned fixed storage.
// This codec neither opens sockets nor invokes a model or the game engine.
inline bool encodeModelIOWireRequest(const ModelIORequest &request,
                                    uint8_t *buffer, size_t capacity, size_t &written) {
  written = 0;
  if (!model_io_wire_detail::validRequest(request)) {
    return false;
  }
  model_io_wire_detail::Writer writer(buffer, capacity);
  if (!model_io_wire_detail::writeHeader(writer, ModelIOWireKind::Request) ||
      !writer.u32(request.schemaVersion) || !writer.u64(request.requestId) ||
      !model_io_wire_detail::writeStamp(writer, request.stamp) ||
      !model_io_wire_detail::writeSelf(writer, request.self) ||
      !writer.i32(request.visibleEnemy.entityIndex) ||
      !writer.vec3(request.visibleEnemy.position) ||
      !writer.u8(request.visibleEnemy.valid ? 1 : 0) ||
      !writer.u8(request.eventCount)) {
    return false;
  }
  for (uint8_t i = 0; i < request.eventCount; ++i) {
    const auto &event = request.events[i];
    if (!writer.u8(static_cast<uint8_t>(event.event.kind)) ||
        !writer.u8(static_cast<uint8_t>(event.event.evidence)) ||
        !writer.i32(event.event.subjectEntityIndex) ||
        !writer.vec3(event.event.position) ||
        !writer.u8(event.hasPosition ? 1 : 0)) {
      return false;
    }
  }
  if (!writer.u8(request.hasFeedback ? 1 : 0)) {
    return false;
  }
  if (request.hasFeedback) {
    const auto &feedback = request.feedback;
    if (!writer.u64(feedback.sessionId) ||
        !writer.u64(feedback.decisionFrameId) ||
        !writer.u64(feedback.observedFrameId) ||
        !writer.u8(static_cast<uint8_t>(feedback.result.action)) ||
        !writer.u8(static_cast<uint8_t>(feedback.result.type)) ||
        !writer.f32(feedback.result.elapsedTime)) {
      return false;
    }
  }
  if (!writer.u32(request.features.schemaVersion) ||
      !writer.u32(static_cast<uint32_t>(kInferenceFeatureCount))) {
    return false;
  }
  for (size_t i = 0; i < kInferenceFeatureCount; ++i) {
    if (!writer.f32(request.features.values[i])) {
      return false;
    }
  }
  return writer.finish(written);
}

inline bool decodeModelIOWireRequest(const uint8_t *buffer, size_t size,
                                    ModelIORequest &request) {
  model_io_wire_detail::Reader reader(buffer, size);
  ModelIORequest decoded {};
  if (!reader.header(ModelIOWireKind::Request) ||
      !reader.u32(decoded.schemaVersion) || !reader.u64(decoded.requestId) ||
      !model_io_wire_detail::readStamp(reader, decoded.stamp) ||
      !model_io_wire_detail::readSelf(reader, decoded.self) ||
      !reader.i32(decoded.visibleEnemy.entityIndex) ||
      !reader.vec3(decoded.visibleEnemy.position) ||
      !model_io_wire_detail::readBoolean(reader, decoded.visibleEnemy.valid) ||
      !reader.u8(decoded.eventCount) ||
      decoded.eventCount > kPerceptionEventsPerFrame) {
    return false;
  }
  for (uint8_t i = 0; i < decoded.eventCount; ++i) {
    auto &event = decoded.events[i];
    uint8_t kind {}, evidence {};
    if (!reader.u8(kind) || !reader.u8(evidence) ||
        !reader.i32(event.event.subjectEntityIndex) ||
        !reader.vec3(event.event.position) ||
        !model_io_wire_detail::readBoolean(reader, event.hasPosition)) {
      return false;
    }
    event.event.stamp = decoded.stamp;
    event.event.kind = static_cast<EventKind>(kind);
    event.event.evidence = static_cast<Evidence>(evidence);
  }
  if (!model_io_wire_detail::readBoolean(reader, decoded.hasFeedback)) {
    return false;
  }
  if (decoded.hasFeedback) {
    auto &feedback = decoded.feedback;
    uint8_t action {}, type {};
    if (!reader.u64(feedback.sessionId) ||
        !reader.u64(feedback.decisionFrameId) ||
        !reader.u64(feedback.observedFrameId) ||
        !reader.u8(action) || !reader.u8(type) ||
        !reader.f32(feedback.result.elapsedTime)) {
      return false;
    }
    feedback.result.action = static_cast<ActionType>(action);
    feedback.result.type = static_cast<ActionResultType>(type);
  }
  uint32_t featureCount {};
  if (!reader.u32(decoded.features.schemaVersion) ||
      !reader.u32(featureCount) ||
      featureCount != kInferenceFeatureCount) {
    return false;
  }
  for (size_t i = 0; i < kInferenceFeatureCount; ++i) {
    if (!reader.f32(decoded.features.values[i])) {
      return false;
    }
  }
  if (!reader.done() || !model_io_wire_detail::validRequest(decoded)) {
    return false;
  }
  request = decoded;
  return true;
}

inline bool encodeModelIOWireResponse(const ModelIOResponse &response,
                                     uint8_t *buffer, size_t capacity, size_t &written) {
  written = 0;
  if (!model_io_wire_detail::validResponse(response)) {
    return false;
  }
  model_io_wire_detail::Writer writer(buffer, capacity);
  const auto &output = response.inference.output;
  if (!model_io_wire_detail::writeHeader(writer, ModelIOWireKind::Response) ||
      !writer.u32(response.schemaVersion) || !writer.u64(response.requestId) ||
      !writer.u64(response.sessionId) || !writer.u64(response.observationFrameId) ||
      !writer.u8(static_cast<uint8_t>(response.inference.status)) ||
      !writer.u32(output.schemaVersion) || !writer.u8(output.actionId) ||
      !writer.i32(output.targetNode) || !writer.i32(output.targetPlayer) ||
      !writer.vec3(output.targetPosition) || !writer.u8(output.weaponType) ||
      !writer.u8(output.grenadeType) || !writer.f32(output.duration) ||
      !writer.f32(output.confidence)) {
    return false;
  }
  return writer.finish(written);
}

// Correlation is checked as part of parsing, before the response is exposed.
inline bool decodeModelIOWireResponse(const uint8_t *buffer, size_t size,
                                     const ModelIORequest &expected, ModelIOResponse &response) {
  model_io_wire_detail::Reader reader(buffer, size);
  ModelIOResponse decoded {};
  uint8_t status {};
  auto &output = decoded.inference.output;
  if (!reader.header(ModelIOWireKind::Response) ||
      !reader.u32(decoded.schemaVersion) || !reader.u64(decoded.requestId) ||
      !reader.u64(decoded.sessionId) || !reader.u64(decoded.observationFrameId) ||
      !reader.u8(status) || !reader.u32(output.schemaVersion) ||
      !reader.u8(output.actionId) || !reader.i32(output.targetNode) ||
      !reader.i32(output.targetPlayer) || !reader.vec3(output.targetPosition) ||
      !reader.u8(output.weaponType) || !reader.u8(output.grenadeType) ||
      !reader.f32(output.duration) || !reader.f32(output.confidence) ||
      !reader.done()) {
    return false;
  }
  decoded.inference.status = static_cast<InferenceStatus>(status);
  if (!model_io_wire_detail::validResponse(decoded) ||
      !modelIOResponseMatches(expected, decoded)) {
    return false;
  }
  response = decoded;
  return true;
}

} // namespace ai::runtime_io
