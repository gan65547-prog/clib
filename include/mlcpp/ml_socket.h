#pragma once

// ── mlcpp::Socket — C++ RAII wrapper for clib ml_stream ─────────────────
//
// Thin RAII wrapper around clib's UDS/TCP socket API. Eliminates raw
// POSIX socket boilerplate from C++ code. All I/O delegates to clib.
//
// Used by domainChannel, outputStage, and domain-runner.
//
// Part of the mlcpp C++ binding layer for clib components.
// Future wrappers: mlcpp::Pool (ml_pool), mlcpp::Container (ml_container).

#include <memory>
#include <cstdint>

#include "ml_stream.h"
#include "ml_code.h"

namespace mlcpp {

class Socket {
public:
    Socket() = default;

    explicit Socket(int fd) : fd_(fd) {}

    ~Socket() { close(); }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) { close(); fd_ = other.fd_; other.fd_ = -1; }
        return *this;
    }

    int fd() const { return fd_; }
    bool valid() const { return fd_ >= 0; }

    // ── Factories ───────────────────────────────────────────────────────

    static std::unique_ptr<Socket> createUnixServer(const char *path,
                                                     int maxListen = 5) {
        int fd = -1;
        if (ml_unix_create(path, maxListen, &fd) != ML_OK)
            return nullptr;
        return std::make_unique<Socket>(fd);
    }

    static std::unique_ptr<Socket> connectUnix(const char *path,
                                                int timeoutMs = 1000) {
        int fd = -1;
        if (ml_unix_connect(path, &fd, timeoutMs) != ML_OK)
            return nullptr;
        return std::make_unique<Socket>(fd);
    }

    static std::unique_ptr<Socket> createTcpServer(const char *addr4,
                                                    uint16_t port,
                                                    int maxListen = 5) {
        int fd = -1;
        if (ml_tcp_create(addr4, port, static_cast<ml_uint_t>(maxListen), &fd) != ML_OK)
            return nullptr;
        return std::make_unique<Socket>(fd);
    }

    static std::unique_ptr<Socket> connectTcp(const char *addr4,
                                               uint16_t port,
                                               int timeoutMs = 1000) {
        int fd = -1;
        if (ml_tcp_connect(addr4, port, &fd, timeoutMs) != ML_OK)
            return nullptr;
        return std::make_unique<Socket>(fd);
    }

    // ── Accept ──────────────────────────────────────────────────────────

    std::unique_ptr<Socket> accept() const {
        int clientFd = ::accept(fd_, nullptr, nullptr);
        if (clientFd < 0) return nullptr;
        return std::make_unique<Socket>(clientFd);
    }

    // ── I/O ─────────────────────────────────────────────────────────────

    bool sendAll(const void *buf, size_t size) const {
        int sent = 0;
        return ml_stream_send(fd_, buf, static_cast<int>(size), &sent) == ML_OK;
    }

    bool recvAll(void *buf, size_t size) const {
        int received = 0;
        return ml_stream_recv(fd_, buf, static_cast<int>(size), &received) == ML_OK;
    }

    bool recvTimeout(void *buf, size_t size, int timeoutMs) const {
        int received = 0;
        return ml_stream_recv_timeout(fd_, buf, static_cast<int>(size),
                                       &received, timeoutMs) == ML_OK;
    }

    void close() {
        if (fd_ >= 0) {
            ml_stream_close(&fd_);
        }
    }

private:
    int fd_ = -1;
};

} // namespace mlcpp
