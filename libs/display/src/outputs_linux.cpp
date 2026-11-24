// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/display/outputs.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace fs = std::filesystem;

namespace zen1::display {
namespace {

std::string readLine(const fs::path& p) {
    std::ifstream in(p);
    std::string s;
    std::getline(in, s);
    return s;
}

bool isInternalConnector(const std::string& c) {
    return c.find("-eDP-") != std::string::npos || c.find("-LVDS-") != std::string::npos ||
           c.find("-DSI-") != std::string::npos;
}

} // namespace

std::vector<Output> scanOutputs(const std::string& drmRoot) {
    std::vector<Output> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(drmRoot, ec)) {
        const std::string n = e.path().filename().string();
        // Connectors look like "card1-DP-1"; skip "card1", "renderD128", "version".
        if (n.rfind("card", 0) != 0 || n.find('-') == std::string::npos) continue;
        if (!fs::exists(e.path() / "status", ec)) continue;

        Output o;
        o.connector = n;
        o.status = readLine(e.path() / "status");
        o.enabled = readLine(e.path() / "enabled") == "enabled";
        o.internal = isInternalConnector(n);

        std::ifstream modes(e.path() / "modes");
        std::string line;
        while (std::getline(modes, line)) {
            int w = 0, h = 0;
            char x = 0;
            std::istringstream ls(line);
            if (ls >> w >> x >> h && x == 'x') {
                const Mode m{w, h};
                const bool dup = std::any_of(o.modes.begin(), o.modes.end(),
                    [&](const Mode& k) { return k.width == w && k.height == h; });
                if (!dup) o.modes.push_back(m);
            }
        }

        std::ifstream edid(e.path() / "edid", std::ios::binary);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(edid)),
                                        std::istreambuf_iterator<char>());
        o.edid = parseEdid(bytes);
        out.push_back(std::move(o));
    }
    std::sort(out.begin(), out.end(),
              [](const Output& a, const Output& b) { return a.connector < b.connector; });
    return out;
}

std::optional<size_t> guessCabinetOutput(const std::vector<Output>& outputs) {
    auto connected = [](const Output& o) { return o.status == "connected"; };
    for (size_t i = 0; i < outputs.size(); ++i) {
        const auto& o = outputs[i];
        if (!connected(o) || o.internal || o.modes.empty()) continue;
        if (o.modes.front().width == 1280 && o.modes.front().height == 1024) return i;
    }
    for (size_t i = 0; i < outputs.size(); ++i)
        if (connected(outputs[i]) && !outputs[i].internal) return i;
    for (size_t i = 0; i < outputs.size(); ++i)
        if (connected(outputs[i])) return i;
    return std::nullopt;
}

} // namespace zen1::display
