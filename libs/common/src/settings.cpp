// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/settings.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace zen1 {
namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

bool validName(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

// Removes a trailing "# comment" that is outside quotes.
std::string stripComment(const std::string& s) {
    bool inStr = false;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '\\' && inStr) { ++i; continue; }
        if (c == '"') inStr = !inStr;
        if (c == '#' && !inStr) return s.substr(0, i);
    }
    return s;
}

std::optional<SettingValue> parseValue(const std::string& raw) {
    const std::string v = trim(raw);
    if (v.empty()) return std::nullopt;
    if (v.front() == '"') {
        if (v.size() < 2 || v.back() != '"') return std::nullopt;
        std::string out;
        for (size_t i = 1; i + 1 < v.size(); ++i) {
            char c = v[i];
            if (c == '\\' && i + 2 < v.size()) {
                const char n = v[++i];
                switch (n) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    default: return std::nullopt;
                }
                continue;
            }
            if (c == '"') return std::nullopt;  // unescaped quote inside
            out += c;
        }
        return SettingValue{out};
    }
    if (v == "true") return SettingValue{true};
    if (v == "false") return SettingValue{false};

    long long i = 0;
    auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), i);
    if (ec == std::errc() && p == v.data() + v.size()) return SettingValue{i};

    // Doubles: from_chars for double is not available in every libstdc++ we
    // target, so use strtod with full-consumption check.
    char* end = nullptr;
    errno = 0;
    const double d = std::strtod(v.c_str(), &end);
    if (errno == 0 && end == v.c_str() + v.size()) return SettingValue{d};
    return std::nullopt;
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    out += '"';
    return out;
}

std::string formatValue(const SettingValue& v) {
    if (const auto* s = std::get_if<std::string>(&v)) return quote(*s);
    if (const auto* b = std::get_if<bool>(&v)) return *b ? "true" : "false";
    if (const auto* i = std::get_if<long long>(&v)) return std::to_string(*i);
    std::ostringstream o;
    o.precision(10);
    o << std::get<double>(v);
    std::string s = o.str();
    if (s.find_first_of(".eE") == std::string::npos) s += ".0";
    return s;
}

std::string sectionOf(const std::string& key) {
    const auto dot = key.find('.');
    return dot == std::string::npos ? std::string{} : key.substr(0, dot);
}

} // namespace

bool Settings::parse(const std::string& text, std::string* error) {
    std::istringstream in(text);
    std::string line, section;
    int lineNo = 0;
    bool ok = true;
    while (std::getline(in, line)) {
        ++lineNo;
        const std::string t = trim(stripComment(line));
        if (t.empty()) continue;
        if (t.front() == '[') {
            if (t.back() != ']' || !validName(trim(t.substr(1, t.size() - 2)))) {
                if (error && ok) *error = "line " + std::to_string(lineNo) + ": bad section header";
                ok = false;
                continue;
            }
            section = trim(t.substr(1, t.size() - 2));
            continue;
        }
        const auto eq = t.find('=');
        if (eq == std::string::npos) {
            if (error && ok) *error = "line " + std::to_string(lineNo) + ": expected key = value";
            ok = false;
            continue;
        }
        const std::string key = trim(t.substr(0, eq));
        const auto value = parseValue(t.substr(eq + 1));
        if (!validName(key) || !value) {
            if (error && ok) *error = "line " + std::to_string(lineNo) + ": invalid key or value";
            ok = false;
            continue;
        }
        set(section.empty() ? key : section + "." + key, *value);
    }
    return ok;
}

bool Settings::load(const fs::path& file, std::string* error) {
    std::error_code ec;
    if (!fs::exists(file, ec)) return true;
    std::ifstream in(file);
    if (!in) {
        if (error) *error = "cannot read " + file.string();
        return false;
    }
    std::stringstream buf;
    buf << in.rdbuf();
    return parse(buf.str(), error);
}

std::string Settings::toText() const {
    std::ostringstream out;
    // Top-level keys first, then each section in first-seen order.
    std::vector<std::string> sections;
    for (const auto& k : order_) {
        const std::string s = sectionOf(k);
        if (s.empty()) {
            out << k << " = " << formatValue(values_.at(k)) << "\n";
        } else if (std::find(sections.begin(), sections.end(), s) == sections.end()) {
            sections.push_back(s);
        }
    }
    for (const auto& s : sections) {
        out << "\n[" << s << "]\n";
        for (const auto& k : order_) {
            if (sectionOf(k) != s) continue;
            out << k.substr(s.size() + 1) << " = " << formatValue(values_.at(k)) << "\n";
        }
    }
    return out.str();
}

bool Settings::save(const fs::path& file, std::string* error) const {
    std::error_code ec;
    if (file.has_parent_path()) fs::create_directories(file.parent_path(), ec);
    const fs::path tmp = file.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            if (error) *error = "cannot write " + tmp.string();
            return false;
        }
        out << "# Zen1 Arcade settings. Edit while Zen1 Arcade is closed.\n";
        out << toText();
        out.flush();
        if (!out) {
            if (error) *error = "write failed for " + tmp.string();
            return false;
        }
    }
    fs::rename(tmp, file, ec);
    if (ec) {
        if (error) *error = "cannot replace " + file.string() + ": " + ec.message();
        return false;
    }
    return true;
}

bool Settings::has(const std::string& key) const { return values_.count(key) != 0; }

std::string Settings::getString(const std::string& key, const std::string& fallback) const {
    auto it = values_.find(key);
    if (it == values_.end()) return fallback;
    if (const auto* s = std::get_if<std::string>(&it->second)) return *s;
    return fallback;
}

long long Settings::getInt(const std::string& key, long long fallback) const {
    auto it = values_.find(key);
    if (it == values_.end()) return fallback;
    if (const auto* i = std::get_if<long long>(&it->second)) return *i;
    if (const auto* d = std::get_if<double>(&it->second)) return static_cast<long long>(*d);
    return fallback;
}

double Settings::getDouble(const std::string& key, double fallback) const {
    auto it = values_.find(key);
    if (it == values_.end()) return fallback;
    if (const auto* d = std::get_if<double>(&it->second)) return *d;
    if (const auto* i = std::get_if<long long>(&it->second)) return static_cast<double>(*i);
    return fallback;
}

bool Settings::getBool(const std::string& key, bool fallback) const {
    auto it = values_.find(key);
    if (it == values_.end()) return fallback;
    if (const auto* b = std::get_if<bool>(&it->second)) return *b;
    return fallback;
}

void Settings::set(const std::string& key, SettingValue value) {
    if (values_.count(key) == 0) order_.push_back(key);
    values_[key] = std::move(value);
}

void Settings::remove(const std::string& key) {
    values_.erase(key);
    order_.erase(std::remove(order_.begin(), order_.end(), key), order_.end());
}

std::vector<std::string> Settings::keysIn(const std::string& section) const {
    std::vector<std::string> out;
    for (const auto& k : order_)
        if (sectionOf(k) == section) out.push_back(k.substr(section.size() + 1));
    return out;
}

} // namespace zen1
