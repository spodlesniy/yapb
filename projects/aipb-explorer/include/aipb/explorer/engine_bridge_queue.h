#pragma once

#include <aipb/explorer/engine_bridge_math.h>

namespace aipb::explorer::bridge {

// A bounded single-bot command program. Engine frames are scheduled externally.
struct QueuedProbe {
  AipbExplorerBridgeCommand command {};
  ProbeBudget budget {};
};

struct ProbeQueue {
  static constexpr int kMaxCommands = 32;

  QueuedProbe commands[kMaxCommands] {};
  int count {};
  int botIndex { -1 };
  int next {};
  bool running {};

  void clear() {
    count = 0;
    botIndex = -1;
    next = 0;
    running = false;
  }

  [[nodiscard]] bool enqueue(
    int requestedBot,
    const AipbExplorerBridgeCommand &command,
    int limit,
    bool milliseconds
  ) {
    const ProbeBudget budget { limit, milliseconds };
    if (running || requestedBot < 0 || count >= kMaxCommands
      || (count != 0 && botIndex != requestedBot)
      || !valid(command) || !budget.valid()) {
      return false;
    }

    if (count == 0) {
      botIndex = requestedBot;
    }
    commands[count++] = { command, budget };
    return true;
  }

  [[nodiscard]] bool start() {
    if (running || count == 0 || botIndex < 0) {
      return false;
    }
    next = 0;
    running = true;
    return true;
  }

  [[nodiscard]] const QueuedProbe *current() const {
    return running && next < count ? &commands[next] : nullptr;
  }

  // Advance without inserting an idle command or a zero-duration engine frame.
  [[nodiscard]] bool advance() {
    if (!current()) {
      return false;
    }
    ++next;
    return current() != nullptr;
  }
};

} // namespace aipb::explorer::bridge
