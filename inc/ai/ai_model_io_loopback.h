//
// AiPB - local-only nonblocking TCP exchange for Model I/O wire v1.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <chrono>
#include <cstddef>
#include <cstdint>

#include <ai/ai_model_io_transport.h>

namespace ai::runtime_io {

namespace loopback_detail {

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
inline int lastError() { return WSAGetLastError(); }
inline bool pending(int error) {
  return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEINTR;
}
inline bool interrupted(int error) { return error == WSAEINTR; }
inline void closeSocket(Socket socket) { closesocket(socket); }
#else
using Socket = int;
constexpr Socket kInvalidSocket = -1;
inline int lastError() { return errno; }
inline bool pending(int error) {
  return error == EINPROGRESS || error == EWOULDBLOCK || error == EAGAIN || error == EINTR;
}
inline bool interrupted(int error) { return error == EINTR; }
inline void closeSocket(Socket socket) { ::close(socket); }
#endif

using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;

enum class WaitResult { Ready, TimedOut, Failed };

class SocketSession final {
private:
  Socket m_socket { kInvalidSocket };
  bool m_ready {};

public:
  SocketSession() {
#ifdef _WIN32
    WSADATA data {};
    m_ready = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    m_ready = true;
#endif
  }

  ~SocketSession() {
    if (m_socket != kInvalidSocket) {
      closeSocket(m_socket);
    }
#ifdef _WIN32
    if (m_ready) {
      WSACleanup();
    }
#endif
  }

  SocketSession(const SocketSession &) = delete;
  SocketSession &operator=(const SocketSession &) = delete;

  Socket open() {
    if (!m_ready) {
      return kInvalidSocket;
    }
    m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == kInvalidSocket) {
      return kInvalidSocket;
    }
#ifdef _WIN32
    u_long enabled = 1;
    if (ioctlsocket(m_socket, FIONBIO, &enabled) != 0) {
      return kInvalidSocket;
    }
#else
    const int flags = fcntl(m_socket, F_GETFL, 0);
    if (flags < 0 || fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
      return kInvalidSocket;
    }
#ifdef SO_NOSIGPIPE
    int enabled = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) != 0) {
      return kInvalidSocket;
    }
#endif
#endif
    return m_socket;
  }
};

inline WaitResult waitFor(Socket socket, bool writable, Deadline deadline) {
#ifndef _WIN32
  if (socket < 0 || socket >= FD_SETSIZE) {
    return WaitResult::Failed;
  }
#endif
  for (;;) {
    const auto now = Clock::now();
    if (now >= deadline) {
      return WaitResult::TimedOut;
    }
    const auto left = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
    timeval timeout {};
    timeout.tv_sec = static_cast<long>(left / 1000000);
    timeout.tv_usec = static_cast<long>(left % 1000000);
    if (left <= 0) {
      timeout.tv_usec = 1;
    }

    fd_set readable {};
    fd_set writeable {};
    FD_ZERO(&readable);
    FD_ZERO(&writeable);
    if (writable) {
      FD_SET(socket, &writeable);
    }
    else {
      FD_SET(socket, &readable);
    }
    const int result = ::select(static_cast<int>(socket) + 1,
        writable ? nullptr : &readable, writable ? &writeable : nullptr,
        nullptr, &timeout);
    if (result > 0) {
      return WaitResult::Ready;
    }
    if (result == 0) {
      continue;
    }
    if (!interrupted(lastError())) {
      return WaitResult::Failed;
    }
  }
}

inline ModelIOExchangeStatus await(Socket socket, bool writable, Deadline deadline) {
  switch (waitFor(socket, writable, deadline)) {
  case WaitResult::Ready:
    return ModelIOExchangeStatus::Completed;
  case WaitResult::TimedOut:
    return ModelIOExchangeStatus::TimedOut;
  case WaitResult::Failed:
    return ModelIOExchangeStatus::Failed;
  }
  return ModelIOExchangeStatus::Failed;
}

inline ModelIOExchangeStatus connect(Socket socket, uint16_t port, Deadline deadline) {
  sockaddr_in address {};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (::connect(socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == 0) {
    return Clock::now() < deadline ? ModelIOExchangeStatus::Completed :
                                     ModelIOExchangeStatus::TimedOut;
  }
  if (!pending(lastError())) {
    return ModelIOExchangeStatus::Unavailable;
  }
  const auto ready = await(socket, true, deadline);
  if (ready != ModelIOExchangeStatus::Completed) {
    return ready;
  }

  int error {};
#ifdef _WIN32
  int length = sizeof(error);
#else
  socklen_t length = sizeof(error);
#endif
  if (getsockopt(socket, SOL_SOCKET, SO_ERROR,
                 reinterpret_cast<char *>(&error), &length) != 0) {
    return ModelIOExchangeStatus::Failed;
  }
  if (error != 0) {
    return ModelIOExchangeStatus::Unavailable;
  }
  return Clock::now() < deadline ? ModelIOExchangeStatus::Completed :
                                   ModelIOExchangeStatus::TimedOut;
}

inline ModelIOExchangeStatus sendAll(Socket socket, const uint8_t *data,
                                    size_t size, Deadline deadline) {
  size_t offset {};
  while (offset < size) {
    const auto ready = await(socket, true, deadline);
    if (ready != ModelIOExchangeStatus::Completed) {
      return ready;
    }
    int flags = 0;
#ifdef MSG_NOSIGNAL
    flags = MSG_NOSIGNAL;
#endif
    const int count = ::send(socket, reinterpret_cast<const char *>(data + offset),
                             static_cast<int>(size - offset), flags);
    if (count > 0) {
      offset += static_cast<size_t>(count);
    }
    else if (count == 0) {
      return ModelIOExchangeStatus::Failed;
    }
    else if (!pending(lastError())) {
      return ModelIOExchangeStatus::Failed;
    }
  }
  return ModelIOExchangeStatus::Completed;
}

inline ModelIOExchangeStatus receiveAll(Socket socket, uint8_t *data,
                                       size_t size, Deadline deadline) {
  size_t offset {};
  while (offset < size) {
    const auto ready = await(socket, false, deadline);
    if (ready != ModelIOExchangeStatus::Completed) {
      return ready;
    }
    const int count = ::recv(socket, reinterpret_cast<char *>(data + offset),
                             static_cast<int>(size - offset), 0);
    if (count > 0) {
      offset += static_cast<size_t>(count);
    }
    else if (count == 0) {
      return ModelIOExchangeStatus::Failed;
    }
    else if (!pending(lastError())) {
      return ModelIOExchangeStatus::Failed;
    }
  }
  return ModelIOExchangeStatus::Completed;
}

} // namespace loopback_detail

// IPv4 loopback is hardwired: callers can select a port, but never an
// external host. Each exchange owns one short-lived socket and one deadline.
class ModelIOLoopbackTcpTransport final : public ModelIOByteTransport {
private:
  uint16_t m_port {};

public:
  explicit ModelIOLoopbackTcpTransport(uint16_t port) : m_port(port) {}

  uint16_t port() const { return m_port; }

  ModelIOExchangeResult exchange(const uint8_t *request, size_t requestBytes,
                                 uint8_t *response, size_t responseCapacity,
                                 uint32_t timeoutMs) const override {
    if (m_port == 0 || request == nullptr || response == nullptr ||
        requestBytes < kModelIOWireHeaderBytes ||
        requestBytes > kModelIOWireMaxBytes ||
        responseCapacity < kModelIOWireHeaderBytes ||
        timeoutMs == 0 || timeoutMs > kModelIOTransportMaxTimeoutMs) {
      return { ModelIOExchangeStatus::Failed, 0 };
    }

    const auto deadline = loopback_detail::Clock::now() +
                          std::chrono::milliseconds(timeoutMs);
    loopback_detail::SocketSession session {};
    const auto socket = session.open();
    if (socket == loopback_detail::kInvalidSocket) {
      return { ModelIOExchangeStatus::Unavailable, 0 };
    }
    auto status = loopback_detail::connect(socket, m_port, deadline);
    if (status != ModelIOExchangeStatus::Completed) {
      return { status, 0 };
    }
    status = loopback_detail::sendAll(socket, request, requestBytes, deadline);
    if (status != ModelIOExchangeStatus::Completed) {
      return { status, 0 };
    }
    status = loopback_detail::receiveAll(socket, response, kModelIOWireHeaderBytes, deadline);
    if (status != ModelIOExchangeStatus::Completed) {
      return { status, 0 };
    }

    const uint32_t payload = static_cast<uint32_t>(response[8]) |
        (static_cast<uint32_t>(response[9]) << 8) |
        (static_cast<uint32_t>(response[10]) << 16) |
        (static_cast<uint32_t>(response[11]) << 24);
    const size_t bound = responseCapacity < kModelIOWireMaxBytes ?
                         responseCapacity : kModelIOWireMaxBytes;
    if (payload == 0 || payload > bound - kModelIOWireHeaderBytes) {
      return { ModelIOExchangeStatus::Failed, 0 };
    }
    const size_t total = kModelIOWireHeaderBytes + payload;
    status = loopback_detail::receiveAll(socket, response + kModelIOWireHeaderBytes,
                                         payload, deadline);
    if (status != ModelIOExchangeStatus::Completed) {
      return { status, 0 };
    }
    if (loopback_detail::Clock::now() >= deadline) {
      return { ModelIOExchangeStatus::TimedOut, 0 };
    }
    return { ModelIOExchangeStatus::Completed, total };
  }
};

class ModelIOSteadyClock final : public ModelIOMonotonicClock {
public:
  uint64_t milliseconds() const override {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        loopback_detail::Clock::now().time_since_epoch()).count());
  }
};

} // namespace ai::runtime_io
