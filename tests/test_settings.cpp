// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/settings.hpp"
#include "zen1_test.hpp"

#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace zen1;
namespace fs = std::filesystem;

TEST("Settings parse sections, types and comments") {
    Settings s;
    std::string err;
    CHECK(s.parse("top = 1\n# comment\n[video]\nmode = \"integer\" # trailing\nzoom = 0.75\n"
                  "bezel = true\n[audio]\nvolume = 80\n", &err));
    CHECK_EQ(s.getInt("top"), 1LL);
    CHECK_EQ(s.getString("video.mode"), std::string("integer"));
    CHECK(s.getDouble("video.zoom") > 0.74 && s.getDouble("video.zoom") < 0.76);
    CHECK(s.getBool("video.bezel"));
    CHECK_EQ(s.getInt("audio.volume"), 80LL);
    CHECK_EQ(s.getInt("missing", 7), 7LL);
}

TEST("Settings report the line of a syntax error but keep valid lines") {
    Settings s;
    std::string err;
    CHECK(!s.parse("a = 1\nthis is wrong\nb = 2\n", &err));
    CHECK(err.find("line 2") != std::string::npos);
    CHECK_EQ(s.getInt("a"), 1LL);
    CHECK_EQ(s.getInt("b"), 2LL);
}

TEST("Settings strings with quotes, backslashes and # survive a round trip") {
    Settings a;
    a.set("p.path", std::string("/dev/input/by-path/pci-0000:04:00.3-usb-0:1.2:1.0-event-joystick"));
    a.set("p.text", std::string("say \"hi\" # not a comment \\ ok"));
    Settings b;
    CHECK(b.parse(a.toText()));
    CHECK_EQ(b.getString("p.path"), a.getString("p.path"));
    CHECK_EQ(b.getString("p.text"), a.getString("p.text"));
}

TEST("Settings save atomically and reload") {
    const fs::path dir = fs::temp_directory_path() / ("zen1-settings-" + std::to_string(::getpid()));
    fs::create_directories(dir);
    const fs::path file = dir / "settings.toml";
    Settings a;
    a.set("display.output", std::string("card1-DP-1"));
    a.set("intro.enabled", true);
    CHECK(a.save(file));
    CHECK(!fs::exists(file.string() + ".tmp"));
    Settings b;
    CHECK(b.load(file));
    CHECK_EQ(b.getString("display.output"), std::string("card1-DP-1"));
    CHECK(b.getBool("intro.enabled"));
    fs::remove_all(dir);
}

TEST("Missing settings file loads as empty") {
    Settings s;
    CHECK(s.load("/nonexistent/zen1/settings.toml"));
    CHECK(!s.has("anything"));
}
