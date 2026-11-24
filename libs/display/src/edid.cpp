// SPDX-License-Identifier: GPL-3.0-or-later
// EDID 1.3/1.4 base block parsing (VESA E-EDID standard layout).
#include "zen1/display/outputs.hpp"

#include <array>

namespace zen1::display {

std::optional<EdidInfo> parseEdid(const std::vector<std::uint8_t>& b) {
    static constexpr std::array<std::uint8_t, 8> kHeader = {0x00, 0xFF, 0xFF, 0xFF,
                                                           0xFF, 0xFF, 0xFF, 0x00};
    if (b.size() < 128) return std::nullopt;
    for (size_t i = 0; i < kHeader.size(); ++i)
        if (b[i] != kHeader[i]) return std::nullopt;
    std::uint8_t sum = 0;
    for (size_t i = 0; i < 128; ++i) sum = static_cast<std::uint8_t>(sum + b[i]);
    if (sum != 0) return std::nullopt;

    EdidInfo e;
    const unsigned m = (static_cast<unsigned>(b[8]) << 8) | b[9];
    e.manufacturer = {static_cast<char>('A' - 1 + ((m >> 10) & 0x1F)),
                      static_cast<char>('A' - 1 + ((m >> 5) & 0x1F)),
                      static_cast<char>('A' - 1 + (m & 0x1F))};
    e.productCode = static_cast<std::uint16_t>(b[10] | (b[11] << 8));
    e.serial = static_cast<std::uint32_t>(b[12]) | (static_cast<std::uint32_t>(b[13]) << 8) |
               (static_cast<std::uint32_t>(b[14]) << 16) | (static_cast<std::uint32_t>(b[15]) << 24);
    e.widthCm = b[21];
    e.heightCm = b[22];

    // Four 18-byte descriptors at 54, 72, 90, 108.
    for (size_t off = 54; off <= 108; off += 18) {
        const std::uint8_t* d = &b[off];
        const unsigned pixelClock = d[0] | (d[1] << 8);
        if (pixelClock != 0) {
            if (!e.preferred) {
                const int hActive = d[2] | ((d[4] & 0xF0) << 4);
                const int vActive = d[5] | ((d[7] & 0xF0) << 4);
                e.preferred = Mode{hActive, vActive};
            }
            continue;
        }
        if (d[3] == 0xFC) {  // monitor name
            std::string name;
            for (int i = 5; i < 18; ++i) {
                if (d[i] == 0x0A) break;
                name += static_cast<char>(d[i]);
            }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            e.monitorName = name;
        }
    }
    return e;
}

} // namespace zen1::display
