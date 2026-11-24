// SPDX-License-Identifier: GPL-3.0-or-later
//
// Frontend <-> engine link: a Unix stream socket owned by the frontend.
// Line-based messages:
//   engine -> frontend: "ready", "exit <reason>", "error <text>"
//   frontend -> engine: "exit", "ping"
#pragma once

#include <optional>
#include <string>

namespace zen1::engine {

class IpcClient {
public:
    ~IpcClient();
    bool connect(const std::string& path);
    bool connected() const { return fd_ >= 0; }
    void send(const std::string& line);
    // Non-blocking: next complete line from the frontend, if any.
    std::optional<std::string> poll();

private:
    int fd_ = -1;
    std::string buffer_;
};

}  // namespace zen1::engine
