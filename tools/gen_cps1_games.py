#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generates machines/cps1/cps1_games_data.inc from MAME's CPS1 driver.

Usage: tools/gen_cps1_games.py /path/to/mame/src/mame/capcom > machines/cps1/cps1_games_data.inc

It reads, for each game listed in GAMES below: the GAME() line (title, year,
CPU clock, rotation), the ROM_START block (files, CRCs, offsets, layouts),
the CPS-B configuration and graphics bank mapper (cps1_v.cpp), and the DIP
switch defaults (INPUT_PORTS). The data is MAME's (BSD-3-Clause,
copyright-holders: Paul Leaman); see machines/cps1/LICENSE-MAME-BSD-3-Clause.txt.
"""
import re
import sys

# set -> (parent, joystick, button names). Only standard CPS1 boards
# (cps1_10MHz / cps1_12MHz, no QSound, no encryption) are listed.
GAMES = {
    "sf2":      ("", "8-way", ["Light Punch", "Medium Punch", "Heavy Punch", "Light Kick", "Medium Kick", "Heavy Kick"]),
    "sf2ce":    ("sf2", "8-way", ["Light Punch", "Medium Punch", "Heavy Punch", "Light Kick", "Medium Kick", "Heavy Kick"]),
    "sf2hf":    ("sf2", "8-way", ["Light Punch", "Medium Punch", "Heavy Punch", "Light Kick", "Medium Kick", "Heavy Kick"]),
    "ffight":   ("", "8-way", ["Attack", "Jump"]),
    "1941":     ("", "8-way", ["Shot", "Loop"]),
    "ghouls":   ("", "8-way", ["Fire", "Jump"]),
    "strider":  ("", "8-way", ["Attack", "Jump"]),
    "dynwar":   ("", "8-way", ["Attack Left", "Attack Right", "Special"]),
    "willow":   ("", "8-way", ["Attack", "Jump"]),
    "unsquad":  ("", "8-way", ["Shot", "Special"]),
    "mercs":    ("", "8-way", ["Shot", "Mega Crush"]),
    "mtwins":   ("", "8-way", ["Attack", "Jump"]),
    "msword":   ("", "8-way", ["Attack", "Jump"]),
    "cawing":   ("", "8-way", ["Shot", "Special"]),
    "nemo":     ("", "8-way", ["Attack", "Jump"]),
    "3wonders": ("", "8-way", ["Attack", "Jump"]),
    "kod":      ("", "8-way", ["Attack", "Jump"]),
    "captcomm": ("", "8-way", ["Attack", "Jump"]),
    "knights":  ("", "8-way", ["Attack", "Jump"]),
    "varth":    ("", "8-way", ["Shot", "Bomb"]),
    "megaman":  ("", "8-way", ["Attack", "Jump", "Special"]),
}

SKIP_REGIONS = {"aboardplds", "bboardplds", "cboardplds", "dboardplds", "stars", "user1"}
LOADS = {
    "ROM_LOAD": "Plain", "ROM_LOAD16_WORD": "Plain", "ROM_LOAD16_BYTE": "Byte16",
    "ROM_LOAD16_WORD_SWAP": "WordSwap16", "ROM_LOAD64_WORD": "Word64", "ROM_LOAD64_BYTE": "Byte64",
}


def num(s):
    s = s.strip()
    return int(s, 16) if s.lower().startswith("0x") else int(s)


def split_args(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "({" and True:
            depth += 1
        if ch in ")}":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def call_args(text, start):
    """Returns (args_string, end_index) for the parenthesised call starting at text[start] == '('."""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return text[start + 1:i], i + 1
    raise ValueError("unbalanced")


def load_macros(src):
    macros = {}
    joined = src.replace("\\\n", " ")
    for m in re.finditer(r"^#define\s+(\w+)(\(([^)]*)\))?[ \t]+(.*)$", joined, re.M):
        params = [p.strip() for p in m.group(3).split(",")] if m.group(2) else None
        macros[m.group(1)] = (params, m.group(4))
    return macros


def expand(text, macros, depth=0):
    if depth > 8:
        return text
    out, i = "", 0
    for m in re.finditer(r"\b([A-Z][A-Z0-9_]+)\b", text):
        pass
    pos = 0
    res = ""
    pattern = re.compile(r"\b([A-Za-z_]\w*)\b")
    while True:
        m = pattern.search(text, pos)
        if not m:
            res += text[pos:]
            break
        name = m.group(1)
        res += text[pos:m.start()]
        if name in macros and macros[name][0] is not None:
            j = m.end()
            while j < len(text) and text[j] in " \t":
                j += 1
            if j < len(text) and text[j] == "(":
                args, end = call_args(text, j)
                params, body = macros[name]
                vals = split_args(args)
                for p, v in zip(params, vals):
                    body = re.sub(r"\b" + re.escape(p) + r"\b", v, body)
                res += expand(body, macros, depth + 1)
                pos = end
                continue
        res += name
        pos = m.end()
    return res


def port_defaults(src, macros, name, ports=None):
    if ports is None:
        ports = {}
    m = re.search(r"INPUT_PORTS_START\(\s*" + re.escape(name) + r"\s*\)(.*?)INPUT_PORTS_END", src, re.S)
    body = expand(m.group(1), macros)
    cur = None
    for t in re.finditer(r"(PORT_INCLUDE|PORT_START|PORT_MODIFY|PORT_DIPNAME|PORT_DIPUNUSED_DIPLOC|PORT_DIPUNUSED|"
                         r"PORT_DIPUNKNOWN_DIPLOC|PORT_DIPUNKNOWN|PORT_SERVICE_DIPLOC|PORT_SERVICE_NO_TOGGLE|PORT_SERVICE|PORT_BIT)\s*\(", body):
        args, _ = call_args(body, t.end() - 1)
        a = split_args(args)
        kind = t.group(1)
        if kind == "PORT_INCLUDE":
            port_defaults(src, macros, a[0], ports)
        elif kind == "PORT_START":
            cur = a[0].strip('"')
            ports[cur] = {"value": 0, "mask": 0, "buttons": 0, "players": set()}
        elif kind == "PORT_MODIFY":
            cur = a[0].strip('"')
            ports.setdefault(cur, {"value": 0, "mask": 0, "buttons": 0, "players": set()})
        elif cur is None:
            continue
        elif kind in ("PORT_SERVICE", "PORT_SERVICE_DIPLOC", "PORT_SERVICE_NO_TOGGLE"):
            mask = num(a[0])
            p = ports[cur]
            p["mask"] |= mask
            p["value"] = (p["value"] & ~mask) | (mask if "LOW" in a[1] else 0)
        elif kind == "PORT_BIT":
            mask = num(a[0])
            p = ports[cur]
            p["mask"] |= mask
            p["value"] = (p["value"] & ~mask) | (mask if "LOW" in a[1] else 0)
            rest = body[t.end():t.end() + 200]
            bm = re.match(r"\s*IPT_BUTTON(\d)", a[2]) if len(a) > 2 else None
            pl = re.search(r"PORT_PLAYER\((\d)\)", body[t.start():t.start() + 160].split("\n")[0])
            if bm and pl and pl.group(1) == "1":
                p["buttons"] = max(p["buttons"], int(bm.group(1)))
            if pl:
                p["players"].add(int(pl.group(1)))
        else:
            mask, default = num(a[0]), num(a[1])
            p = ports[cur]
            p["mask"] |= mask
            p["value"] = (p["value"] & ~mask) | (default & mask)
    return ports


def dip(ports, name):
    p = ports.get(name)
    if not p:
        return 0xff
    return (p["value"] | (~p["mask"] & 0xff)) & 0xff


def main(capcom):
    src = open(capcom + "/cps1.cpp").read()
    vsrc = open(capcom + "/cps1_v.cpp").read()
    macros = load_macros(src)

    # CPS-B macros
    cpsb = {}
    for m in re.finditer(r"^#define\s+(CPS_B_\w+)\s+(.*)$", vsrc, re.M):
        body = m.group(2).split("//")[0].replace("__not_applicable__", "-1,-1,-1,-1,-1,-1,-1")
        vals = [num(x) for x in re.findall(r"-?0x[0-9a-fA-F]+|-?\d+", body)]
        cpsb[m.group(1)] = vals
    # mappers
    mapper_sizes = {}
    for m in re.finditer(r"^#define\s+(mapper_\w+)\s+\{([^}]*)\},\s*(mapper_\w+_table)", vsrc, re.M):
        mapper_sizes[m.group(1)] = ([num(x) for x in m.group(2).split(",")], m.group(3))
    tables = {}
    for m in re.finditer(r"static const struct gfx_range (mapper_\w+_table)\[\]\s*=\s*\{(.*?)\};", vsrc, re.S):
        rows = []
        for r in re.finditer(r"\{\s*(GFXTYPE_[A-Z0-9_ |]+?)\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(\d+)\s*\}",
                             re.sub(r"//.*", "", m.group(2))):
            types = [t.strip().replace("GFXTYPE_", "GFX_") for t in r.group(1).split("|")]
            types = [t for t in types if t != "GFX_STARS"]
            if not types:
                continue
            rows.append((" | ".join(types), r.group(2), r.group(3), r.group(4)))
        tables[m.group(1)] = rows
    config = {}
    for m in re.finditer(r'\{"(\w+)",\s*(CPS_B_\w+),\s*(mapper_\w+)\s*(?:,([^}]*))?\}', vsrc):
        extra = [num(x) for x in re.findall(r"0x[0-9a-fA-F]+|\d+", m.group(4) or "")]
        config[m.group(1)] = (m.group(2), m.group(3), extra)

    print("// Generated by tools/gen_cps1_games.py from MAME's CPS1 driver (BSD-3-Clause,")
    print("// copyright-holders: Paul Leaman). Do not edit by hand.")
    for game, (parent, joystick, names) in GAMES.items():
        gl = re.search(r"^GAME\(\s*(\d+),\s*" + re.escape(game) + r"\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*\w+\s*,\s*(\w+)\s*,\s*(ROT\d+)\s*,\s*\"([^\"]*)\"\s*,\s*\"([^\"]*)\"",
                       src, re.M)
        year, mparent, machine, inputs, init, rot, maker, title = gl.groups()
        assert machine in ("cps1_10MHz", "cps1_12MHz"), (game, machine)
        assert init == "empty_init", (game, init)
        if mparent != "0":
            assert mparent == parent, (game, mparent)
        clock = 12_000_000 if machine == "cps1_12MHz" else 10_000_000
        b, mp, extra = config[game]
        v = cpsb[b]
        sizes, table = mapper_sizes[mp]
        ports = port_defaults(src, macros, inputs)
        buttons = ports.get("IN1", {}).get("buttons", 0)
        players = 2
        for pn, p in ports.items():
            players = max(players, max(p["players"]) if p["players"] else 0)
        clean_title = re.sub(r"\s*\([^)]*\)\s*$", "", title)

        roms, region, regions = [], None, {}
        block = re.search(r"ROM_START\(\s*" + re.escape(game) + r"\s*\)(.*?)ROM_END", src, re.S).group(1)
        for line in block.splitlines():
            r = re.search(r'ROM_REGION\w*\(\s*([^,]+),\s*"(\w+)"', line)
            if r:
                region = r.group(2)
                size = r.group(1).strip()
                regions[region] = 0x400000 if size == "CODE_SIZE" else num(size)
                continue
            if region in SKIP_REGIONS or region is None:
                continue
            r = re.search(r'(ROM_LOAD\w*)\(\s*"([^"]+)",\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+),\s*CRC\((\w+)\)', line)
            if r:
                kind = LOADS[r.group(1)]
                roms.append(f'{{"{region}", "{r.group(2)}", 0x{r.group(5).lower()}, {r.group(3)}, {r.group(4)}, RomLoad::{kind}}}')
                continue
            r = re.search(r"ROM_CONTINUE\(\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)", line)
            if r:
                roms.append(f'{{"{region}", nullptr, 0, {r.group(1)}, {r.group(2)}, RomLoad::Continue}}')
        rows = tables[table]
        in2 = extra[0] if len(extra) > 0 and extra[0] else -1
        print(f"\n{{   // {title}")
        print("    GameDef g;")
        names_cpp = ", ".join(f'"{n}"' for n in names)
        print(f'    g.info.set = "{game}"; g.info.parent = "{parent}"; g.info.title = "{clean_title}";')
        print(f'    g.info.year = "{year}"; g.info.manufacturer = "{maker}"; g.info.system = "Capcom CPS-1";')
        print(f'    g.info.players = {players}; g.info.buttons = {len(names)}; g.info.rotation = {rot[3:]};')
        print(f'    g.info.joystick = "{joystick}";')
        print(f"    g.info.buttonNames = {{{names_cpp}}};")
        print(f"    g.cpuClock = {clock};")
        print(f"    g.board.cpsbAddr = {v[0]}; g.board.cpsbValue = {v[1]};")
        print(f"    g.board.multFactor1 = {v[2]}; g.board.multFactor2 = {v[3]}; g.board.multLo = {v[4]}; g.board.multHi = {v[5]};")
        print(f"    g.board.layerControl = {hex(v[9])}; g.board.priority = {{{', '.join(hex(x) for x in v[10:14])}}};"
              f" g.board.paletteControl = {hex(v[14])};")
        print(f"    g.board.layerEnable = {{{', '.join(hex(x) for x in v[15:20])}}};")
        print(f"    g.board.bankSizes = {{{', '.join(hex(x) for x in sizes)}}};")
        print("    g.board.mapper = {" + ", ".join(f"{{{t}, {s}, {e}, {bk}}}" for t, s, e, bk in rows) + "};")
        print(f"    g.board.in2Addr = {in2};")
        print(f"    g.dips = {{{hex(dip(ports, 'DSWA'))}, {hex(dip(ports, 'DSWB'))}, {hex(dip(ports, 'DSWC'))}}};")
        print(f"    g.gfxRegionSize = {hex(regions.get('gfx', 0))};")
        print("    g.roms = {")
        for r in roms:
            print(f"        {r},")
        print("    };")
        print("    d.push_back(std::move(g));")
        print("}")


if __name__ == "__main__":
    main(sys.argv[1])
