//
// AiPB - bounded external Model I/O wire codec tests.
//
// SPDX-License-Identifier: MIT
//

#include "ai_test.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include <ai/ai_model_io_wire.h>

using ai::test::expect;
namespace io = ai::runtime_io;

namespace {

io::ModelIORequest makeWireRequest() {
  io::ModelIORequest request {};
  request.requestId = 42;
  request.stamp.sessionId = 3;
  request.stamp.frameId = 12;
  request.stamp.roundId = 2;
  request.stamp.botEntityIndex = 4;
  request.stamp.gameTime = 1.5f;
  request.stamp.frameSeconds = 0.02f;
  request.self.origin = { 10.0f, 20.0f, 30.0f };
  request.self.velocity = { 4.0f, -2.0f, 0.0f };
  request.self.viewAngles = { 0.0f, 90.0f, 0.0f };
  request.self.health = 100.0f;
  request.self.armor = 40.0f;
  request.self.maxSpeed = 250.0f;
  request.self.alive = true;
  request.self.onGround = true;
  request.visibleEnemy = { 7, { 50.0f, 60.0f, 70.0f }, true };
  request.eventCount = 2;
  request.events[0].event.stamp = request.stamp;
  request.events[0].event.kind = io::EventKind::EntityAppeared;
  request.events[0].event.evidence = io::Evidence::Seen;
  request.events[0].event.subjectEntityIndex = 7;
  request.events[0].event.position = request.visibleEnemy.position;
  request.events[0].hasPosition = true;
  request.events[1].event.stamp = request.stamp;
  request.events[1].event.kind = io::EventKind::Sound;
  request.events[1].event.evidence = io::Evidence::Heard;
  request.events[1].event.subjectEntityIndex = -1;
  request.events[1].event.position = { 20.0f, 30.0f, 40.0f };
  request.events[1].hasPosition = true;
  request.hasFeedback = true;
  request.feedback.sessionId = 3;
  request.feedback.decisionFrameId = 10;
  request.feedback.observedFrameId = 11;
  request.feedback.result.action = ai::ActionType::MoveToPosition;
  request.feedback.result.type = ai::ActionResultType::Completed;
  request.feedback.result.elapsedTime = 0.4f;
  request.features.values[0] = 0.25f;
  request.features.values[ai::kInferenceFeatureCount - 1] = -0.5f;
  return request;
}

io::ModelIOResponse makeWireResponse(const io::ModelIORequest &request) {
  io::ModelIOResponse response {};
  response.requestId = request.requestId;
  response.sessionId = request.stamp.sessionId;
  response.observationFrameId = request.stamp.frameId;
  response.inference.status = ai::InferenceStatus::Success;
  response.inference.output.actionId = static_cast<uint8_t>(ai::InferenceActionId::MoveToPosition);
  response.inference.output.targetPosition = { 100.0f, 200.0f, 30.0f };
  response.inference.output.confidence = 0.95f;
  return response;
}

void setU32(uint8_t *buffer, size_t offset, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    buffer[offset + static_cast<size_t>(i)] = static_cast<uint8_t>(value >> (i * 8));
  }
}

} // namespace

AI_TEST(testModelIOWireRequestRoundTripAndFixedLayout) {
  const auto request = makeWireRequest();
  uint8_t buffer[io::kModelIOWireMaxBytes] {};
  size_t written {};
  expect(io::encodeModelIOWireRequest(request, buffer, sizeof(buffer), written),
         "valid model request must fit fixed maximum");
  expect(written > io::kModelIOWireHeaderBytes && written <= sizeof(buffer),
         "request serialized into bounded buffer");
  expect(buffer[0] == 'A' && buffer[1] == 'I' && buffer[2] == 'P' &&
         buffer[3] == 'B' && buffer[4] == 1 && buffer[5] == 0 &&
         buffer[6] == 1 && buffer[7] == 0, "canonical little-endian header");
  const uint32_t payload = static_cast<uint32_t>(buffer[8]) |
      (static_cast<uint32_t>(buffer[9]) << 8) |
      (static_cast<uint32_t>(buffer[10]) << 16) |
      (static_cast<uint32_t>(buffer[11]) << 24);
  expect(payload == written - io::kModelIOWireHeaderBytes,
         "header declares exact payload length");
  io::ModelIORequest decoded {};
  expect(io::decodeModelIOWireRequest(buffer, written, decoded),
         "complete wire request parses");
  expect(decoded.requestId == 42 && decoded.stamp.sessionId == 3 &&
         decoded.stamp.frameId == 12 && decoded.stamp.botEntityIndex == 4,
         "request/frame correlation survives codec");
  expect(decoded.self.origin.z == 30.0f && decoded.self.onGround &&
         decoded.visibleEnemy.valid && decoded.visibleEnemy.entityIndex == 7,
         "authorized self and visible enemy survive codec");
  expect(decoded.eventCount == 2 &&
         decoded.events[0].event.kind == io::EventKind::EntityAppeared &&
         decoded.events[1].event.kind == io::EventKind::Sound &&
         decoded.events[1].event.subjectEntityIndex == -1,
         "only authorized event evidence survives codec");
  expect(decoded.events[0].event.stamp.frameId == 12,
         "events inherit enclosing validated frame stamp");
  expect(decoded.hasFeedback && decoded.feedback.decisionFrameId == 10 &&
         decoded.feedback.result.type == ai::ActionResultType::Completed,
         "terminal feedback survives codec");
  expect(decoded.features.values[0] == 0.25f &&
         decoded.features.values[ai::kInferenceFeatureCount - 1] == -0.5f,
         "entire fixed feature vector survives codec");
  uint8_t again[io::kModelIOWireMaxBytes] {};
  size_t repeated {};
  expect(io::encodeModelIOWireRequest(decoded, again, sizeof(again), repeated) &&
         repeated == written && std::memcmp(buffer, again, written) == 0,
         "wire representation remains byte-for-byte deterministic");
}

AI_TEST(testModelIOWireRejectsMalformedRequest) {
  auto request = makeWireRequest();
  uint8_t buffer[io::kModelIOWireMaxBytes] {};
  size_t written {};
  expect(io::encodeModelIOWireRequest(request, buffer, sizeof(buffer), written),
         "valid fixture encoded");
  io::ModelIORequest decoded {};
  expect(!io::decodeModelIOWireRequest(buffer, written - 1, decoded),
         "truncation fails closed");
  expect(!io::decodeModelIOWireRequest(buffer, written + 1, decoded),
         "trailing bytes fail closed");
  buffer[4] = 2;
  expect(!io::decodeModelIOWireRequest(buffer, written, decoded),
         "unsupported wire version rejected");
  buffer[4] = 1;
  buffer[6] = 2;
  expect(!io::decodeModelIOWireRequest(buffer, written, decoded),
         "wrong message kind rejected");
  buffer[6] = 1;
  setU32(buffer, 8, 0xffffffffu);
  expect(!io::decodeModelIOWireRequest(buffer, written, decoded),
         "declared payload length is verified");
  setU32(buffer, 8, static_cast<uint32_t>(written - io::kModelIOWireHeaderBytes));
  // Header 12 + schema 4 + request ID 8 + stamp schema 4.
  setU32(buffer, 12, 99);
  expect(!io::decodeModelIOWireRequest(buffer, written, decoded),
         "unsupported Model I/O schema rejected");
  setU32(buffer, 12, io::kModelIOSchemaVersion);

  // bool event payload or physical state is checked before any model use.
  request.features.values[0] = std::numeric_limits<float>::quiet_NaN();
  expect(!io::encodeModelIOWireRequest(request, buffer, sizeof(buffer), written) &&
         written == 0, "nonfinite features cannot be exported");
  request = makeWireRequest();
  request.events[1].event.subjectEntityIndex = 7;
  expect(!io::encodeModelIOWireRequest(request, buffer, sizeof(buffer), written),
         "deanonymized sound cannot be exported");
  request = makeWireRequest();
  expect(!io::encodeModelIOWireRequest(request, buffer, 32, written),
         "insufficient caller storage rejected");
}

AI_TEST(testModelIOWireResponseCorrelationAndErrors) {
  const auto request = makeWireRequest();
  auto response = makeWireResponse(request);
  uint8_t buffer[io::kModelIOWireMaxBytes] {};
  size_t written {};
  expect(io::encodeModelIOWireResponse(response, buffer, sizeof(buffer), written),
         "valid response serialized");
  io::ModelIOResponse decoded {};
  expect(io::decodeModelIOWireResponse(buffer, written, request, decoded),
         "matching reply accepted");
  expect(decoded.requestId == request.requestId &&
         decoded.inference.output.targetPosition.y == 200.0f &&
         decoded.inference.output.confidence == 0.95f,
         "action content and correlation preserved");

  auto wrongRequest = request;
  ++wrongRequest.requestId;
  expect(!io::decodeModelIOWireResponse(buffer, written, wrongRequest, decoded),
         "foreign request ID rejected");
  wrongRequest = request;
  ++wrongRequest.stamp.sessionId;
  expect(!io::decodeModelIOWireResponse(buffer, written, wrongRequest, decoded),
         "foreign session rejected");
  wrongRequest = request;
  ++wrongRequest.stamp.frameId;
  expect(!io::decodeModelIOWireResponse(buffer, written, wrongRequest, decoded),
         "wrong observation frame rejected");
  expect(!io::decodeModelIOWireResponse(buffer, written - 1, request, decoded),
         "truncated reply rejected");
  buffer[6] = 1;
  expect(!io::decodeModelIOWireResponse(buffer, written, request, decoded),
         "request message cannot be decoded as response");
  buffer[6] = 2;

  // Response layout: 12-byte header + 4-byte schema + 3 u64 identifiers.
  buffer[40] = 255;
  expect(!io::decodeModelIOWireResponse(buffer, written, request, decoded),
         "unsupported response status rejected");
  buffer[40] = static_cast<uint8_t>(ai::InferenceStatus::Success);
  response.inference.output.confidence = std::numeric_limits<float>::infinity();
  expect(!io::encodeModelIOWireResponse(response, buffer, sizeof(buffer), written),
         "nonfinite output rejected before transport");
}

AI_TEST(testModelIOWireNoDecisionAndUnchangedOutputOnFailure) {
  const auto request = makeWireRequest();
  auto response = makeWireResponse(request);
  response.inference.status = ai::InferenceStatus::NoDecision;
  uint8_t buffer[io::kModelIOWireMaxBytes] {};
  size_t written {};
  expect(io::encodeModelIOWireResponse(response, buffer, sizeof(buffer), written),
         "NoDecision round trip allowed");
  io::ModelIOResponse decoded {};
  expect(io::decodeModelIOWireResponse(buffer, written, request, decoded) &&
         decoded.inference.status == ai::InferenceStatus::NoDecision,
         "NoDecision remains explicit");

  decoded.requestId = 999;
  buffer[0] = 0;
  expect(!io::decodeModelIOWireResponse(buffer, written, request, decoded) &&
         decoded.requestId == 999, "malformed reply does not partially mutate caller output");
  io::ModelIORequest requestDecoded {};
  requestDecoded.requestId = 999;
  expect(!io::decodeModelIOWireRequest(buffer, written, requestDecoded) &&
         requestDecoded.requestId == 999,
         "malformed request does not partially mutate caller output");
}

AI_TEST(testModelIOWireV2SafeDirectionMaskAndVersionIsolation) {
  auto request = makeWireRequest();
  request.safeDirectionsMask = 0x05u; // Forward and right; left is not clear.
  uint8_t buffer[io::kModelIOWireMaxBytes] {};
  size_t written {};
  expect(io::encodeModelIOWireRequest(request, buffer, sizeof(buffer), written) &&
         buffer[4] == 2 && buffer[5] == 0 && buffer[written - 1] == 0x05u,
         "v2 request explicitly carries the bounded three-bit direction mask");
  io::ModelIORequest decoded {};
  expect(io::decodeModelIOWireRequest(buffer, written, decoded) &&
         decoded.safeDirectionsMask == 0x05u && decoded.requestId == request.requestId,
         "v2 mask survives canonical encoding without altering request identity");
  expect(!io::decodeModelIOWireRequest(buffer, written - 1, decoded),
         "missing v2 affordance byte is rejected");
  buffer[written - 1] = 0;
  expect(!io::decodeModelIOWireRequest(buffer, written, decoded),
         "v2 cannot advertise an empty safe-direction set");
  buffer[written - 1] = 0x08u;
  expect(!io::decodeModelIOWireRequest(buffer, written, decoded),
         "v2 rejects any unauthorized direction bit");
  request.safeDirectionsMask = 0x80u;
  expect(!io::encodeModelIOWireRequest(request, buffer, sizeof(buffer), written),
         "encoder rejects unsupported bits before I/O");
  request.safeDirectionsMask = 0;
  expect(io::encodeModelIOWireRequest(request, buffer, sizeof(buffer), written) &&
         buffer[4] == 1 && io::decodeModelIOWireRequest(buffer, written, decoded) &&
         decoded.safeDirectionsMask == 0,
         "legacy v1 payload remains unchanged when no affordances are supplied");

  request.safeDirectionsMask = 0x05u;
  auto response = makeWireResponse(request);
  response.wireVersion = io::kModelIOWireAffordanceVersion;
  expect(io::encodeModelIOWireResponse(response, buffer, sizeof(buffer), written) &&
         buffer[4] == 2, "v2 response echoes the new request's wire version");
  io::ModelIOResponse received {};
  expect(io::decodeModelIOWireResponse(buffer, written, request, received) &&
         received.wireVersion == 2, "v2 response preserves existing correlation checks");
  auto legacyRequest = request;
  legacyRequest.safeDirectionsMask = 0;
  expect(!io::decodeModelIOWireResponse(buffer, written, legacyRequest, received),
         "legacy request cannot accept a v2 response");
  response.wireVersion = io::kModelIOWireVersion;
  expect(io::encodeModelIOWireResponse(response, buffer, sizeof(buffer), written) &&
         !io::decodeModelIOWireResponse(buffer, written, request, received),
         "v2 requester cannot accept a downgraded v1 response");
}
