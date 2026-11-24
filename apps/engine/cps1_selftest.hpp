// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "zen1/emu/machine.hpp"

#include <memory>

namespace zen1::engine {

// The CPS1 hardware running a Zen1-made test program (no ROM files needed).
std::unique_ptr<emu::Machine> makeCps1SelfTest();

}  // namespace zen1::engine
