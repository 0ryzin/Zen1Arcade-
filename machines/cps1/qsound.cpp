// SPDX-License-Identifier: GPL-3.0-or-later
//
// Capcom QSound (DL-1425 DSP), high-level emulation adapted from MAME's
// qsoundhle.cpp (license BSD-3-Clause, copyright-holders: superctr, Valley
// Bell; "based on disassembled DSP code"). The voice, ADPCM, echo, filter and
// delay logic is MAME's; the device glue was rewritten for Zen1.
//
// The DSP's own 4 kword ROM (dl-1425.bin, in MAME's qsound.zip) holds the
// pan, filter and ADPCM tables. Zen1 never ships it: with the user's
// qsound.zip the tables are exact; without it Zen1 uses its own tables
// (constant-power pan, no Q1 filter, standard ADPCM steps), which play every
// voice but lose the QSound 3D effect.
#include "qsound.hpp"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace zen1::cps1 {

QSound::QSound() {
    init_register_map();
    useApproximateTables();
    reset();
}

void QSound::setDspRom(const std::vector<uint8_t>& rom) {
    // MAME region is 16-bit little-endian words (ROM_LOAD16_WORD).
    if (rom.size() < 0x2000) return;
    for (std::size_t i = 0; i < 0x1000; ++i) m_dsp_rom[i] = static_cast<uint16_t>(rom[2 * i] | (rom[2 * i + 1] << 8));
    m_exact = true;
}

void QSound::useApproximateTables() {
    std::fill(std::begin(m_dsp_rom), std::end(m_dsp_rom), 0);
    // Pan: entries DATA_PAN_TAB + 0..0x20 (0x10 = centre), dry table, then
    // the right channel PAN_TABLE_CH_OFFSET later. Voices are subtracted.
    for (int p = 0; p <= 0x20; ++p) {
        const double a = (p / 32.0) * 1.5707963267948966;
        const double l = std::cos(a), r = std::sin(a);
        m_dsp_rom[(DATA_PAN_TAB + p + PAN_TABLE_DRY) & 0xfff] = static_cast<uint16_t>(static_cast<int16_t>(-l * 0x2000));
        m_dsp_rom[(DATA_PAN_TAB + p + PAN_TABLE_DRY + PAN_TABLE_CH_OFFSET) & 0xfff] = static_cast<uint16_t>(static_cast<int16_t>(-r * 0x2000));
    }
    // ADPCM step multipliers (x/64), indexed by the signed nibble -8..7.
    static const int steps[8] = {57, 57, 57, 57, 77, 102, 128, 153};
    for (int s = -8; s < 8; ++s) m_dsp_rom[(DATA_ADPCM_TAB + 8 + s) & 0xfff] = static_cast<uint16_t>(steps[std::abs(s < 0 ? -s - 1 : s) & 7]);
    m_exact = false;
}

void QSound::reset() {
    m_ready_flag = 0;
    m_out[0] = m_out[1] = 0;
    m_state = STATE_BOOT;
    m_state_counter = 0;
    m_data_latch = 0;
}

void QSound::generate(int16_t& left, int16_t& right) {
    update_sample();
    left = m_out[0];
    right = m_out[1];
}

void QSound::update_sample() {
	switch (m_state)
	{
		default:
		case STATE_INIT1:
		case STATE_INIT2:
			return state_init();
		case STATE_REFRESH1:
			return state_refresh_filter_1();
		case STATE_REFRESH2:
			return state_refresh_filter_2();
		case STATE_NORMAL1:
		case STATE_NORMAL2:
			return state_normal_update();
	}
}

void QSound::qsound_w(uint32_t offset, uint8_t data)
{
	switch (offset)
	{
		case 0:
			m_data_latch = (m_data_latch & 0x00ff) | (data << 8);
			break;

		case 1:
			m_data_latch = (m_data_latch & 0xff00) | data;
			break;

		case 2:
			write_data(data, m_data_latch);
			break;

		default:
			break;
	}
}



void QSound::write_data(uint8_t address, uint16_t data)
{
	uint16_t *destination = m_register_map[address];
	if (destination)
		*destination = data;
	if (m_ready_flag)
	{
		m_ready_flag = 0;
	}
}

void QSound::init_register_map()
{
	// unused registers
	std::fill(std::begin(m_register_map), std::end(m_register_map), nullptr);

	// PCM registers
	for (int i = 0; i < 16; i++) // PCM voices
	{
		m_register_map[(i << 3) + 0] = (uint16_t*)&m_voice[(i + 1) % 16].m_bank; // Bank applies to the next channel
		m_register_map[(i << 3) + 1] = (uint16_t*)&m_voice[i].m_addr; // Current sample position and start position.
		m_register_map[(i << 3) + 2] = (uint16_t*)&m_voice[i].m_rate; // 4.12 fixed point decimal.
		m_register_map[(i << 3) + 3] = (uint16_t*)&m_voice[i].m_phase;
		m_register_map[(i << 3) + 4] = (uint16_t*)&m_voice[i].m_loop_len;
		m_register_map[(i << 3) + 5] = (uint16_t*)&m_voice[i].m_end_addr;
		m_register_map[(i << 3) + 6] = (uint16_t*)&m_voice[i].m_volume;
		m_register_map[(i << 3) + 7] = nullptr; // unused
		m_register_map[i + 0x80] = (uint16_t*)&m_voice_pan[i];
		m_register_map[i + 0xba] = (uint16_t*)&m_voice[i].m_echo;
	}

	// ADPCM registers
	for (int i = 0; i < 3; i++) // ADPCM voices
	{
		// ADPCM sample rate is fixed to 8khz. (one channel is updated every third sample)
		m_register_map[(i << 2) + 0xca] = (uint16_t*)&m_adpcm[i].m_start_addr;
		m_register_map[(i << 2) + 0xcb] = (uint16_t*)&m_adpcm[i].m_end_addr;
		m_register_map[(i << 2) + 0xcc] = (uint16_t*)&m_adpcm[i].m_bank;
		m_register_map[(i << 2) + 0xcd] = (uint16_t*)&m_adpcm[i].m_volume;
		m_register_map[i + 0xd6] = (uint16_t*)&m_adpcm[i].m_flag; // non-zero to start ADPCM playback
		m_register_map[i + 0x90] = (uint16_t*)&m_voice_pan[16 + i];
	}

	// QSound registers
	m_register_map[0x93] = (uint16_t*)&m_echo.m_feedback;
	m_register_map[0xd9] = (uint16_t*)&m_echo.m_end_pos;
	m_register_map[0xe2] = (uint16_t*)&m_delay_update; // non-zero to update delays
	m_register_map[0xe3] = (uint16_t*)&m_next_state;
	for (int i = 0; i < 2; i++)  // left, right
	{
		// Wet
		m_register_map[(i << 1) + 0xda] = (uint16_t*)&m_filter[i].m_table_pos;
		m_register_map[(i << 1) + 0xde] = (uint16_t*)&m_wet[i].m_delay;
		m_register_map[(i << 1) + 0xe4] = (uint16_t*)&m_wet[i].m_volume;
		// Dry
		m_register_map[(i << 1) + 0xdb] = (uint16_t*)&m_alt_filter[i].m_table_pos;
		m_register_map[(i << 1) + 0xdf] = (uint16_t*)&m_dry[i].m_delay;
		m_register_map[(i << 1) + 0xe5] = (uint16_t*)&m_dry[i].m_volume;
	}
}

int16_t QSound::read_sample(uint16_t bank, uint16_t address)
{
	bank &= 0x7fff;
	const uint32_t rom_addr = (bank << 16) | (address << 0);
	const uint8_t sample_data = m_samples && !m_samples->empty() ? (*m_samples)[rom_addr % m_samples->size()] : 0;
	return (int16_t)(sample_data << 8); // bit0-7 is tied to ground
}

/********************************************************************/

// Initialization routine
void QSound::state_init()
{
	int mode = (m_state == STATE_INIT2) ? 1 : 0;

	// we're busy for 4 samples, including the filter refresh.
	if (m_state_counter >= 2)
	{
		m_state_counter = 0;
		m_state = m_next_state;
		return;
	}
	else if (m_state_counter == 1)
	{
		m_state_counter++;
		return;
	}

	std::fill(std::begin(m_voice), std::end(m_voice), qsound_voice());
	std::fill(std::begin(m_adpcm), std::end(m_adpcm), qsound_adpcm());
	std::fill(std::begin(m_filter), std::end(m_filter), qsound_fir());
	std::fill(std::begin(m_alt_filter), std::end(m_alt_filter), qsound_fir());
	std::fill(std::begin(m_wet), std::end(m_wet), qsound_delay());
	std::fill(std::begin(m_dry), std::end(m_dry), qsound_delay());
	m_echo = qsound_echo();

	for (int i = 0; i < 19; i++)
	{
		m_voice_pan[i] = DATA_PAN_TAB + 0x10;
		m_voice_output[i] = 0;
	}

	for (int i = 0; i < 16; i++)
		m_voice[i].m_bank = 0x8000;
	for (int i = 0; i < 3; i++)
		m_adpcm[i].m_bank = 0x8000;

	if (mode == 0)
	{
		// mode 1
		m_wet[0].m_delay = 0;
		m_dry[0].m_delay = 46;
		m_wet[1].m_delay = 0;
		m_dry[1].m_delay = 48;
		m_filter[0].m_table_pos = DATA_FILTER_TAB + (FILTER_ENTRY_SIZE*1);
		m_filter[1].m_table_pos = DATA_FILTER_TAB + (FILTER_ENTRY_SIZE*2);
		m_echo.m_end_pos = DELAY_BASE_OFFSET + 6;
		m_next_state = STATE_REFRESH1;
	}
	else
	{
		// mode 2
		m_wet[0].m_delay = 1;
		m_dry[0].m_delay = 0;
		m_wet[1].m_delay = 0;
		m_dry[1].m_delay = 0;
		m_filter[0].m_table_pos = 0xf73;
		m_filter[1].m_table_pos = 0xfa4;
		m_alt_filter[0].m_table_pos = 0xf73;
		m_alt_filter[1].m_table_pos = 0xfa4;
		m_echo.m_end_pos = DELAY_BASE_OFFSET2 + 6;
		m_next_state = STATE_REFRESH2;
	}

	m_wet[0].m_volume = 0x3fff;
	m_dry[0].m_volume = 0x3fff;
	m_wet[1].m_volume = 0x3fff;
	m_dry[1].m_volume = 0x3fff;

	m_delay_update = 1;
	if (m_ready_flag)
	{
		m_ready_flag = 0;
	}
	m_state_counter = 1;
}

// Updates filter parameters for mode 1
void QSound::state_refresh_filter_1()
{
	for (int ch = 0; ch < 2; ch++)
	{
		m_filter[ch].m_delay_pos = 0;
		m_filter[ch].m_tap_count = 95;

		for (int i = 0; i < 95; i++)
			m_filter[ch].m_taps[i] = read_dsp_rom(m_filter[ch].m_table_pos + i);
	}

	m_state = m_next_state = STATE_NORMAL1;
}

// Updates filter parameters for mode 2
void QSound::state_refresh_filter_2()
{
	for (int ch = 0; ch < 2; ch++)
	{
		m_filter[ch].m_delay_pos = 0;
		m_filter[ch].m_tap_count = 45;

		for (int i = 0; i < 45; i++)
			m_filter[ch].m_taps[i] = (int16_t)read_dsp_rom(m_filter[ch].m_table_pos + i);

		m_alt_filter[ch].m_delay_pos = 0;
		m_alt_filter[ch].m_tap_count = 44;

		for (int i = 0; i < 44; i++)
			m_alt_filter[ch].m_taps[i] = (int16_t)read_dsp_rom(m_alt_filter[ch].m_table_pos + i);
	}

	m_state = m_next_state = STATE_NORMAL2;
}

// Updates a PCM voice. There are 16 voices, each are updated every sample
// with full rate and volume control.
int16_t QSound::qsound_voice::update(QSound &dsp, int32_t *echo_out)
{
	// Read sample from rom and apply volume
	const int16_t output = (m_volume * dsp.read_sample(m_bank, m_addr)) >> 14;

	*echo_out += (output * m_echo) << 2;

	// Add delta to the phase and loop back if required
	int32_t new_phase = m_rate + ((m_addr << 12) | (m_phase >> 4));

	if ((new_phase >> 12) >= m_end_addr)
		new_phase -= (m_loop_len << 12);

	new_phase = std::clamp<int32_t>(new_phase, -0x8000000, 0x7FFFFFF);
	m_addr = new_phase >> 12;
	m_phase = (new_phase << 4)&0xffff;

	return output;
}

// Updates an ADPCM voice. There are 3 voices, one is updated every sample
// (effectively making the ADPCM rate 1/3 of the master sample rate), and
// volume is set when starting samples only.
// The ADPCM algorithm is supposedly similar to Yamaha ADPCM. It also seems
// like Capcom never used it, so this was not emulated in the earlier QSound
// emulators.
int16_t QSound::qsound_adpcm::update(QSound &dsp, int16_t curr_sample, int nibble)
{
	int8_t step;
	if (!nibble)
	{
		// Mute voice when it reaches the end address.
		if (m_cur_addr == m_end_addr)
			m_cur_vol = 0;

		// Playback start flag
		if (m_flag)
		{
			curr_sample = 0;
			m_flag = 0;
			m_step_size = 10;
			m_cur_vol = m_volume;
			m_cur_addr = m_start_addr;
		}

		// get top nibble
		step = dsp.read_sample(m_bank, m_cur_addr) >> 8;
	}
	else
	{
		// get bottom nibble
		step = dsp.read_sample(m_bank, m_cur_addr++) >> 4;
	}

	// shift with sign extend
	step >>= 4;

	// delta = (0.5 + abs(step)) * m_step_size
	int32_t delta = ((1 + abs(step << 1)) * m_step_size) >> 1;
	if (step <= 0)
		delta = -delta;
	delta += curr_sample;
	delta = std::clamp<int32_t>(delta, -32768, 32767);

	m_step_size = (dsp.read_dsp_rom(DATA_ADPCM_TAB + 8 + step) * m_step_size) >> 6;
	m_step_size = std::clamp<int16_t>(m_step_size, 1, 2000);

	return (delta * m_cur_vol) >> 16;
}

// The echo effect is pretty simple. A moving average filter is used on
// the output from the delay line to smooth samples over time.
int16_t QSound::qsound_echo::apply(int32_t input)
{
	// get average of last 2 samples from the delay line
	int32_t old_sample = m_delay_line[m_delay_pos];
	const int32_t last_sample = m_last_sample;
	m_last_sample = old_sample;
	old_sample = (old_sample + last_sample) >> 1;

	// add current sample to the delay line
	int32_t new_sample = input + ((old_sample * m_feedback) << 2);
	m_delay_line[m_delay_pos++] = new_sample >> 16;

	if (m_delay_pos >= m_length)
		m_delay_pos = 0;

	return old_sample;
}

// Process a sample update
void QSound::state_normal_update()
{
	if (!m_ready_flag)
	{
		m_ready_flag = 0x80;
	}

	// recalculate echo length
	if (m_state == STATE_NORMAL2)
		m_echo.m_length = m_echo.m_end_pos - DELAY_BASE_OFFSET2;
	else
		m_echo.m_length = m_echo.m_end_pos - DELAY_BASE_OFFSET;

	m_echo.m_length = std::clamp<int16_t>(m_echo.m_length, 0, 1024);

	// update PCM voices
	int32_t echo_input = 0;
	for (int i = 0; i < 16; i++)
		m_voice_output[i] = m_voice[i].update(*this, &echo_input);

	// update ADPCM voices (one every third sample)
	const int adpcm_voice = m_state_counter % 3;
	m_voice_output[16 + adpcm_voice] = m_adpcm[adpcm_voice].update(*this, m_voice_output[16 + adpcm_voice], m_state_counter / 3);

	int16_t echo_output = m_echo.apply(echo_input);

	// now, we do the magic stuff
	for (int ch = 0; ch < 2; ch++)
	{
		// Echo is output on the unfiltered component of the left channel and
		// the filtered component of the right channel.
		int32_t wet = (ch == 1) ? echo_output << 14 : 0;
		int32_t dry = (ch == 0) ? echo_output << 14 : 0;

		for (int i = 0; i < 19; i++)
		{
			uint16_t pan_index = m_voice_pan[i] + (ch * PAN_TABLE_CH_OFFSET);

			// Apply different volume tables on the dry and wet inputs.
			dry -= (m_voice_output[i] * (int16_t)read_dsp_rom(pan_index + PAN_TABLE_DRY));
			wet -= (m_voice_output[i] * (int16_t)read_dsp_rom(pan_index + PAN_TABLE_WET));
		}
		// Saturate accumulated voices
		dry = std::clamp<int32_t>(dry, -0x1fffffff, 0x1fffffff) << 2;
		wet = std::clamp<int32_t>(wet, -0x1fffffff, 0x1fffffff) << 2;

		// Apply FIR filter on 'wet' input
		wet = m_filter[ch].apply(wet >> 16);

		// in mode 2, we do this on the 'dry' input too
		if (m_state == STATE_NORMAL2)
			dry = m_alt_filter[ch].apply(dry >> 16);

		// output goes through a delay line and attenuation
		int32_t output = (m_wet[ch].apply(wet) + m_dry[ch].apply(dry));

		// DSP round function
		output = (output + 0x2000) & ~0x3fff;
		m_out[ch] = std::clamp<int32_t>(output >> 14, -0x7fff, 0x7fff);

		if (m_delay_update)
		{
			m_wet[ch].update();
			m_dry[ch].update();
		}
	}

	m_delay_update = 0;

	// after 6 samples, the next state is executed.
	m_state_counter++;
	if (m_state_counter > 5)
	{
		m_state_counter = 0;
		m_state = m_next_state;
	}
}

// Apply the FIR filter used as the Q1 transfer function
int32_t QSound::qsound_fir::apply(int16_t input)
{
	int32_t output = 0, tap = 0;
	for (; tap < (m_tap_count - 1); tap++)
	{
		output -= (m_taps[tap] * m_delay_line[m_delay_pos++]) << 2;

		if (m_delay_pos >= m_tap_count - 1)
			m_delay_pos = 0;
	}

	output -= (m_taps[tap] * input) << 2;

	m_delay_line[m_delay_pos++] = input;
	if (m_delay_pos >= m_tap_count - 1)
		m_delay_pos = 0;

	return output;
}

// Apply delay line and component volume
int32_t QSound::qsound_delay::apply(const int32_t input)
{
	m_delay_line[m_write_pos++] = input >> 16;
	if (m_write_pos >= 51)
		m_write_pos = 0;

	const int32_t output = m_delay_line[m_read_pos++] * m_volume;
	if (m_read_pos >= 51)
		m_read_pos = 0;

	return output;
}

// Update the delay read position to match new delay length
void QSound::qsound_delay::update()
{
	const int16_t new_read_pos = (m_write_pos - m_delay) % 51;
	if (new_read_pos < 0)
		m_read_pos = new_read_pos + 51;
	else
		m_read_pos = new_read_pos;
}

}  // namespace zen1::cps1
