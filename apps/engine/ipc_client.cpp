// SPDX-License-Identifier: GPL-3.0-or-later
#include "ipc_client.hpp"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace zen1::engine {

IpcClient::~IpcClient() {
    if (fd_ >= 0) ::close(fd_);
}

bool IpcClient::connect(const std::string& path) {
    sockaddr_un addr{};
    if (path.size() >= sizeof addr.sun_path) return false;
    fd_ = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd_ < 0) return false;
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
    if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    ::fcntl(fd_, F_SETFL, ::fcntl(fd_, F_GETFL) | O_NONBLOCK);
    return true;
}

void IpcClient::send(const std::string& line) {
    if (fd_ < 0) return;
    const std::string msg = line + "\n";
    // Messages are tiny; a short write means the frontend is gone.
    if (::send(fd_, msg.data(), msg.size(), MSG_NOSIGNAL) != static_cast<ssize_t>(msg.size())) {
        ::close(fd_);
        fd_ = -1;
    }
}

std::optional<std::string> IpcClient::poll() {
    if (fd_ < 0) return std::nullopt;
    char buf[256];
    for (;;) {
        const ssize_t n = ::recv(fd_, buf, sizeof buf, 0);
        if (n > 0) {
            buffer_.append(buf, static_cast<size_t>(n));
            continue;
        }
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            ::close(fd_);
            fd_ = -1;
        }
        break;
    }
    const auto nl = buffer_.find('\n');
    if (nl == std::string::npos) return std::nullopt;
    std::string line = buffer_.substr(0, nl);
    buffer_.erase(0, nl + 1);
    return line;
}

}  // namespace zen1::engine
