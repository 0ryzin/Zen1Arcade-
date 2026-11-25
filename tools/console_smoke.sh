#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Console smoke tests: runs freely available test and homebrew cartridges
# (fetched from their own repositories at test time, never bundled with
# Zen1) and Zen1's own generated test cartridges through zen1-engine with
# no screen, and records evidence: a screenshot after a set number of
# frames, the sound level, and scripted button presses.
#
# usage: tools/console_smoke.sh ENGINE OUTPUT_DIR [WORK_DIR]
# Writes OUTPUT_DIR/results.md and OUTPUT_DIR/*.bmp. Exits non-zero if the
# engine fails on a cartridge it could load (a crash or an error exit).
set -u
ENGINE=$(realpath "$1")
OUT=$(realpath -m "$2")
WORK=$(realpath -m "${3:-$OUT/../console-roms}")
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT" "$WORK"
export SDL_VIDEO_DRIVER=${SDL_VIDEO_DRIVER:-offscreen} SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy}
export SDL_AUDIO_DRIVER=dummy SDL_AUDIODRIVER=dummy

# Fetch only the folders needed from a repository (sparse, shallow).
fetch() {  # fetch OWNER/REPO DIR PATH...
    local repo=$1 dir=$WORK/$2
    shift 2
    [ -d "$dir/.git" ] && return 0
    git clone -q --depth 1 --filter=blob:none --sparse "https://github.com/$repo" "$dir" 2>/dev/null || return 1
    git -C "$dir" sparse-checkout set --no-cone "$@" 2>/dev/null
}
fetch christopherpow/nes-test-roms nes-test-roms '/instr_test-v5/*'
fetch PeterLemon/NES PeterLemon-NES '/HelloWorld/*' '/APU/Twinkle3Channel/*'
fetch retrio/gb-test-roms gb-test-roms '/cpu_instrs/*'
fetch PeterLemon/GB PeterLemon-GB '/HelloWorld/*' '/APU/Twinkle4Channel/*'
fetch PeterLemon/SNES PeterLemon-SNES '/HelloWorld/*' '/CPUTest/CPU/ADC/*' '/PPU/Mode7/*' '/SPC700/ItaloTest/*'
fetch PeterLemon/SMS PeterLemon-SMS '/VDP/HelloWorld/*' '/PSG/SMSAudio/*'
fetch PeterLemon/PCE PeterLemon-PCE '/PSG/PlaySample/*'
fetch dshadoff/PCE_Mouse_Test PCE_Mouse_Test '/src/pce_mouse.pce'
fetch Stephane-D/SGDK SGDK '/sample/basics/hello-world/out/release/rom.bin' \
    '/sample/game/platformer/out/release/rom.bin' '/sample/snd/xgm-player/out/release/rom.bin' \
    '/sample/bitmap/cube-3D/out/release/rom.bin'
python3 "$HERE/make_test_roms.py" "$WORK/zen1" > /dev/null
# Mega Drive .bin files are recognised inside a folder that says so.
mkdir -p "$WORK/MegaDrive"
for s in basics/hello-world game/platformer snd/xgm-player bitmap/cube-3D; do
    f="$WORK/SGDK/sample/$s/out/release/rom.bin"
    [ -f "$f" ] && cp "$f" "$WORK/MegaDrive/sgdk-$(basename "$s").bin"
done

failed=0
{
    echo "# Console smoke tests"
    echo
    echo "| System | Cartridge (source) | Frames | Input | Result | Sound (rms / peak) | Screenshot |"
    echo "| --- | --- | --- | --- | --- | --- | --- |"
} > "$OUT/results.md"

run() {  # run SYSTEM NAME FILE FRAMES INPUT SOURCE
    local system=$1 name=$2 file=$3 frames=$4 input=$5 source=$6
    if [ ! -f "$file" ]; then
        echo "| $system | $name ($source) | - | - | not fetched | - | - |" >> "$OUT/results.md"
        return
    fi
    local args=(--rom "$file" --windowed --size 640x480 --frames "$frames" --audio-report --screenshot "$OUT/$name.bmp")
    [ -n "$input" ] && args+=(--input-script "$input")
    local log
    log=$(timeout 300 "$ENGINE" "${args[@]}" 2>&1)
    local code=$?
    local audio
    audio=$(echo "$log" | sed -n 's/^audio: [0-9]* samples, rms \([0-9.]*\), peak \([0-9]*\)$/\1 \/ \2/p')
    local result="ran"
    if [ $code -ne 0 ]; then
        result="FAILED (exit $code)"
        failed=1
        echo "$log" | tail -5
    fi
    echo "| $system | $name ($source) | $frames | ${input:--} | $result | ${audio:--} | $name.bmp |" >> "$OUT/results.md"
    echo "$system $name: $result, sound ${audio:--}"
}

run NES nes-instr-test "$WORK/nes-test-roms/instr_test-v5/all_instrs.nes" 3000 "" "blargg instr_test-v5"
run NES nes-hello "$WORK/PeterLemon-NES/HelloWorld/HelloWorld.nes" 120 "" "PeterLemon/NES"
run NES nes-twinkle "$WORK/PeterLemon-NES/APU/Twinkle3Channel/Twinkle3Channel.nes" 600 "" "PeterLemon/NES"
run "Game Boy" gb-cpu-instrs "$WORK/gb-test-roms/cpu_instrs/cpu_instrs.gb" 3600 "" "blargg cpu_instrs"
run "Game Boy" gb-twinkle "$WORK/PeterLemon-GB/APU/Twinkle4Channel/Twinkle4Channel.gb" 600 "" "PeterLemon/GB"
run "Game Boy Color" gbc-zen1-test "$WORK/zen1/zen1-test-gbc.gbc" 60 "" "Zen1 tools/make_test_roms.py"
run SNES snes-cpu-adc "$WORK/PeterLemon-SNES/CPUTest/CPU/ADC/CPUADC.sfc" 300 "" "PeterLemon/SNES"
run SNES snes-hello "$WORK/PeterLemon-SNES/HelloWorld/HelloWorld.sfc" 120 "" "PeterLemon/SNES"
run SNES snes-mode7 "$(ls "$WORK"/PeterLemon-SNES/PPU/Mode7/*/*.sfc 2>/dev/null | head -1)" 300 "" "PeterLemon/SNES"
run SNES snes-spc700 "$WORK/PeterLemon-SNES/SPC700/ItaloTest/ItaloTest.sfc" 600 "" "PeterLemon/SNES"
run "Master System" sms-hello "$WORK/PeterLemon-SMS/VDP/HelloWorld/HelloWorld.sms" 120 "" "PeterLemon/SMS"
run "Master System" sms-psg "$WORK/PeterLemon-SMS/PSG/SMSAudio/SMSAudio.sms" 600 "60:right,200-560:lk" "PeterLemon/SMS"
run "Game Gear" gg-zen1-test "$WORK/zen1/zen1-test-gg.gg" 60 "" "Zen1 tools/make_test_roms.py"
run "PC Engine" pce-input "$WORK/PCE_Mouse_Test/src/pce_mouse.pce" 120 "60-130:lk+right+hk" "dshadoff/PCE_Mouse_Test"
run "PC Engine" pce-sample "$WORK/PeterLemon-PCE/PSG/PlaySample/PlaySample.pce" 300 "" "PeterLemon/PCE"
run "Mega Drive" md-hello "$WORK/MegaDrive/sgdk-hello-world.bin" 300 "" "SGDK sample"
run "Mega Drive" md-cube3d "$WORK/MegaDrive/sgdk-cube-3D.bin" 300 "" "SGDK sample"
run "Mega Drive" md-platformer "$WORK/MegaDrive/sgdk-platformer.bin" 600 "100-400:right,250:lk" "SGDK sample"
run "Mega Drive" md-xgm-player "$WORK/MegaDrive/sgdk-xgm-player.bin" 1200 "200:lk" "SGDK sample"
exit $failed
