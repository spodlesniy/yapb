#include <cstdio>
#include <stdexcept>

#include <aipb/explorer/engine_bridge_queue.h>

using namespace aipb::explorer::bridge;

static void require(bool yes) {
  if (!yes) {
    throw std::runtime_error("queue contract failed");
  }
}

int main() {
  try {
    ProbeQueue queue {};
    const AipbExplorerBridgeCommand forward { 1.0f, 0.0f, 0.0f, 0, 0 };
    const AipbExplorerBridgeCommand strafe { 0.0f, 1.0f, 0.0f, 0, 0 };
    require(!queue.start() && !queue.advance() && !queue.current());
    require(queue.enqueue(1, forward, 60, false));
    require(queue.enqueue(1, strafe, 40, false));
    require(!queue.enqueue(2, forward, 10, false));
    require(!queue.enqueue(1, forward, 0, false));
    require(queue.count == 2 && queue.start());
    require(!queue.start() && !queue.enqueue(1, forward, 10, false));
    require(queue.current()->command.forward == 1.0f);
    require(queue.advance() && queue.current()->command.strafe == 1.0f);
    require(!queue.advance() && !queue.current());
    queue.clear();
    require(queue.count == 0 && queue.botIndex == -1 && !queue.running);
    for (int i = 0; i < ProbeQueue::kMaxCommands; ++i) {
      require(queue.enqueue(1, forward, 128, false));
    }
    require(!queue.enqueue(1, forward, 128, false));
    require(queue.count == ProbeQueue::kMaxCommands);
    queue.clear();
    require(queue.enqueue(3, strafe, 1000, true));
    require(queue.commands[0].budget.milliseconds);
    return 0;
  }
  catch (const std::exception &e) {
    std::fprintf(stderr, "aipb_explorer_queue: %s\n", e.what());
    return 1;
  }
}
