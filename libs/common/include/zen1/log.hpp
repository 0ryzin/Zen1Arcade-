// SPDX-License-Identifier: GPL-3.0-or-later
//
// Minimal logger: stderr plus an optional per-session file in Logs/.
// Keeps the 10 most recent session logs.
#pragma once

#include <filesystem>
#include <string>

namespace zen1::log {

enum class Level { Debug, Info, Warn, Error };

// Opens Logs/<program>-<timestamp>.log and prunes old logs of that program.
// Safe to skip; without it, messages go to stderr only.
bool openSessionLog(const std::filesystem::path& logsDir, const std::string& program);

void setMinLevel(Level level);
void write(Level level, const std::string& message);

inline void debug(const std::string& m) { write(Level::Debug, m); }
inline void info(const std::string& m) { write(Level::Info, m); }
inline void warn(const std::string& m) { write(Level::Warn, m); }
inline void error(const std::string& m) { write(Level::Error, m); }

} // namespace zen1::log
