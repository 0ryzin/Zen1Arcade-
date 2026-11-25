// SPDX-License-Identifier: GPL-3.0-or-later
//
// Capcom CPS-2 program decryption (adapted from MAME, BSD-3-Clause; see
// cps2crypt.cpp for the authors).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace zen1::cps1::cps2crypt {

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

// rom: program words (host order); dec: decrypted opcodes, same size.
// length in bytes; limits in words.
void decrypt(const uint16_t* rom, uint16_t* dec, int length, const uint32_t* masterKey, uint32_t lowerLimit,
             uint32_t upperLimit);

// Reads the 20-byte .key file of a MAME CPS-2 set. Limits are byte addresses.
bool keyFromFile(const std::vector<uint8_t>& file, uint32_t key[2], uint32_t& lower, uint32_t& upper);

}  // namespace zen1::cps1::cps2crypt
