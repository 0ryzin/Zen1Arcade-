// SPDX-License-Identifier: GPL-3.0-or-later
//
// Settings stored as a small, readable subset of TOML:
//   # comment
//   [section]
//   key = "string" | 123 | 1.5 | true/false
// Keys are addressed as "section.key". Saving rewrites the file in key
// order; comments are not preserved.
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace zen1 {

using SettingValue = std::variant<std::string, long long, double, bool>;

class Settings {
public:
    // Loads a file. Missing file = empty settings (ok). Returns false and fills
    // `error` (with line number) on a syntax error; valid lines still load.
    bool load(const std::filesystem::path& file, std::string* error = nullptr);

    // Parses text (same rules as load).
    bool parse(const std::string& text, std::string* error = nullptr);

    // Writes atomically (temp file + rename) so a power cut never leaves a
    // half-written file.
    bool save(const std::filesystem::path& file, std::string* error = nullptr) const;

    std::string toText() const;

    bool has(const std::string& key) const;
    std::string getString(const std::string& key, const std::string& fallback = {}) const;
    long long getInt(const std::string& key, long long fallback = 0) const;
    double getDouble(const std::string& key, double fallback = 0.0) const;
    bool getBool(const std::string& key, bool fallback = false) const;

    void set(const std::string& key, SettingValue value);
    void remove(const std::string& key);

    // Keys of one section, e.g. keysIn("player1") -> {"by_path", ...}.
    std::vector<std::string> keysIn(const std::string& section) const;

private:
    // Insertion order of sections and keys is kept for stable files.
    std::vector<std::string> order_;
    std::map<std::string, SettingValue> values_;
};

} // namespace zen1
