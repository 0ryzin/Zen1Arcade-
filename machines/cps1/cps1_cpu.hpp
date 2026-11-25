// SPDX-License-Identifier: GPL-3.0-or-later
// The CPS-1 / CPS-2 68000 bus adapter (private to the module).
#pragma once

#include "cps1.hpp"

#include <Moira.h>

namespace zen1::cps1 {

// ---------------------------------------------------------------- 68000 ---

struct Cps1Machine::Cpu68k : moira::Moira {
    Cps1Machine* m = nullptr;
    moira::u8 read8(moira::u32 a) const override { return m->read8(a & 0xffffff); }
    moira::u16 read16(moira::u32 a) const override {
        a &= 0xffffff;
        // CPS-2: program-space reads (instruction words, PC-relative data)
        // come from the decrypted copy of the ROM.
        if (!m->decryptedOpcodes().empty() && a < 0x400000 && (getFC() & 3) == 2)
            return m->decryptedOpcodes()[a >> 1];
        return m->read16(a);
    }
    void write8(moira::u32 a, moira::u8 v) const override { m->write8(a & 0xffffff, v); }
    void write16(moira::u32 a, moira::u16 v) const override { m->write16(a & 0xffffff, v); }
    // The 68000 has a 16-bit bus: long accesses are two word accesses.
    moira::u32 read32(moira::u32 a) const override {
        return (static_cast<moira::u32>(read16(a)) << 16) | read16(a + 2);
    }
    void write32(moira::u32 a, moira::u32 v) const override {
        write16(a, static_cast<moira::u16>(v >> 16));
        write16(a + 2, static_cast<moira::u16>(v));
    }
    // Interrupt acknowledge cycle: CPS1 clears its IRQ lines here and the
    // 68000 uses the autovector.
    moira::IrqMode readIrqVector(moira::u8, moira::u8*) const override {
        m->acknowledgeIrq();
        const_cast<Cpu68k*>(this)->setIPL(0);
        return moira::IrqMode::AUTO;
    }
};

}  // namespace zen1::cps1
