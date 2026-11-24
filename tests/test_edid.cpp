// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/display/outputs.hpp"
#include "zen1_test.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace zen1::display;
namespace fs = std::filesystem;

namespace {
// Builds a valid 128-byte EDID: manufacturer "ZEN", 1280x1024 preferred,
// monitor name "ARCADE17".
std::vector<std::uint8_t> sampleEdid() {
    std::vector<std::uint8_t> b(128, 0);
    const std::uint8_t header[8] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
    std::memcpy(b.data(), header, 8);
    const unsigned m = ((('Z' - 'A' + 1) & 0x1F) << 10) | ((('E' - 'A' + 1) & 0x1F) << 5) |
                       (('N' - 'A' + 1) & 0x1F);
    b[8] = static_cast<std::uint8_t>(m >> 8);
    b[9] = static_cast<std::uint8_t>(m & 0xFF);
    b[10] = 0x34; b[11] = 0x12;  // product 0x1234
    b[21] = 34; b[22] = 27;      // 34 x 27 cm
    // Detailed timing at 54: pixel clock non-zero, 1280 x 1024.
    b[54] = 0x30; b[55] = 0x2A;
    b[56] = 1280 & 0xFF; b[58] = static_cast<std::uint8_t>((1280 >> 8) << 4);
    b[59] = 1024 & 0xFF; b[61] = static_cast<std::uint8_t>((1024 >> 8) << 4);
    // Monitor name descriptor at 72.
    b[72] = 0; b[73] = 0; b[74] = 0; b[75] = 0xFC; b[76] = 0;
    const char* name = "ARCADE17\n   ";
    std::memcpy(&b[77], name, 13);
    std::uint8_t sum = 0;
    for (int i = 0; i < 127; ++i) sum = static_cast<std::uint8_t>(sum + b[i]);
    b[127] = static_cast<std::uint8_t>(256 - sum);
    return b;
}
}  // namespace

TEST("EDID: manufacturer, size, preferred mode and name are decoded") {
    const auto e = parseEdid(sampleEdid());
    CHECK(e.has_value());
    CHECK_EQ(e->manufacturer, std::string("ZEN"));
    CHECK_EQ(e->productCode, static_cast<std::uint16_t>(0x1234));
    CHECK_EQ(e->monitorName, std::string("ARCADE17"));
    CHECK_EQ(e->widthCm, 34);
    CHECK(e->preferred.has_value());
    CHECK_EQ(e->preferred->width, 1280);
    CHECK_EQ(e->preferred->height, 1024);
}

TEST("EDID: bad checksum or short data is rejected") {
    auto b = sampleEdid();
    b[127] = static_cast<std::uint8_t>(b[127] + 1);
    CHECK(!parseEdid(b).has_value());
    CHECK(!parseEdid({0x00, 0xFF}).has_value());
}

TEST("Cabinet output: external 1280x1024 screen preferred over the built-in panel") {
    std::vector<Output> outs(3);
    outs[0].connector = "card1-eDP-1"; outs[0].status = "connected"; outs[0].internal = true;
    outs[0].modes = {Mode{800, 1280}};
    outs[1].connector = "card1-DP-1"; outs[1].status = "connected"; outs[1].modes = {Mode{1920, 1080}};
    outs[2].connector = "card1-DP-2"; outs[2].status = "connected"; outs[2].modes = {Mode{1280, 1024}};
    CHECK(guessCabinetOutput(outs) == std::optional<size_t>(2));
    outs[2].status = "disconnected";
    CHECK(guessCabinetOutput(outs) == std::optional<size_t>(1));
}

TEST("Scanning a fake /sys/class/drm tree reads status, modes and EDID") {
    const fs::path root = fs::temp_directory_path() / ("zen1-drm-" + std::to_string(::getpid()));
    fs::remove_all(root);
    const fs::path c = root / "card1-DP-1";
    fs::create_directories(c);
    std::ofstream(c / "status") << "connected\n";
    std::ofstream(c / "enabled") << "enabled\n";
    std::ofstream(c / "modes") << "1280x1024\n1280x1024\n1024x768\n";
    const auto edid = sampleEdid();
    std::ofstream(c / "edid", std::ios::binary).write(reinterpret_cast<const char*>(edid.data()), 128);
    fs::create_directories(root / "card1");  // not a connector

    const auto outs = scanOutputs(root.string());
    CHECK_EQ(outs.size(), static_cast<size_t>(1));
    CHECK_EQ(outs[0].modes.size(), static_cast<size_t>(2));  // duplicates removed
    CHECK(outs[0].enabled);
    CHECK(!outs[0].internal);
    CHECK(outs[0].edid.has_value());
    fs::remove_all(root);
}
