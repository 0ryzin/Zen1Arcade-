// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/log.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <vector>

namespace fs = std::filesystem;

namespace zen1::log {
namespace {

std::mutex g_mutex;
std::ofstream g_file;
Level g_min = Level::Info;

const char* name(Level l) {
    switch (l) {
        case Level::Debug: return "debug";
        case Level::Info: return "info";
        case Level::Warn: return "warn";
        case Level::Error: return "error";
    }
    return "info";
}

std::string timestamp(const char* fmt) {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, fmt, &tm);
    return buf;
}

} // namespace

bool openSessionLog(const fs::path& logsDir, const std::string& program) {
    std::lock_guard lock(g_mutex);
    std::error_code ec;
    fs::create_directories(logsDir, ec);

    // Prune: keep the 9 newest so the new one makes 10.
    std::vector<fs::path> old;
    for (const auto& e : fs::directory_iterator(logsDir, ec)) {
        const auto n = e.path().filename().string();
        if (n.rfind(program + "-", 0) == 0 && e.path().extension() == ".log") old.push_back(e.path());
    }
    std::sort(old.begin(), old.end());  // timestamped names sort chronologically
    while (old.size() > 9) {
        fs::remove(old.front(), ec);
        old.erase(old.begin());
    }

    g_file.open(logsDir / (program + "-" + timestamp("%Y%m%d-%H%M%S") + ".log"));
    return g_file.is_open();
}

void setMinLevel(Level level) {
    std::lock_guard lock(g_mutex);
    g_min = level;
}

void write(Level level, const std::string& message) {
    std::lock_guard lock(g_mutex);
    if (level < g_min) return;
    const std::string line = timestamp("%H:%M:%S") + " [" + name(level) + "] " + message;
    std::fprintf(stderr, "%s\n", line.c_str());
    if (g_file.is_open()) {
        g_file << line << '\n';
        g_file.flush();
    }
}

} // namespace zen1::log
