// SPDX-License-Identifier: GPL-3.0-or-later
//
// Capcom QSound, high-level emulation adapted from MAME (BSD-3-Clause,
// superctr and Valley Bell). See qsound.cpp.
#pragma once

#include <cstdint>
#include <vector>

namespace zen1::cps1 {

using std::int16_t;
using std::int32_t;
using std::uint16_t;
using std::uint32_t;
using std::uint8_t;

class QSound {
public:
    QSound();
    static constexpr double kSampleRate = 60'000'000.0 / 2 / 1248;  // about 24038 Hz

    void setSamples(const std::vector<uint8_t>* rom) { m_samples = rom; }
    void setDspRom(const std::vector<uint8_t>& rom);  // dl-1425.bin from qsound.zip
    bool exactTables() const { return m_exact; }
    void reset();

    // Z80 side: 0xd000-0xd002 write, 0xd007 read.
    void qsound_w(uint32_t offset, uint8_t data);
    uint8_t qsound_r() const { return m_ready_flag; }

    void generate(int16_t& left, int16_t& right);

private:
	enum {
		DATA_PAN_TAB        = 0x110,
		DATA_ADPCM_TAB      = 0x9dc,
		DATA_FILTER_TAB     = 0xd53,    // dual filter mode, 5 tables * 95 taps each
		DATA_FILTER_TAB2    = 0xf2e,    // overlapping data (95+15+95)

		STATE_BOOT          = 0x000,
		STATE_INIT1         = 0x288,
		STATE_INIT2         = 0x61a,
		STATE_REFRESH1      = 0x039,
		STATE_REFRESH2      = 0x04f,
		STATE_NORMAL1       = 0x314,
		STATE_NORMAL2       = 0x6b2
	};

	const uint16_t PAN_TABLE_DRY = 0;
	const uint16_t PAN_TABLE_WET = 98;
	const uint16_t PAN_TABLE_CH_OFFSET = 196;
	const uint16_t FILTER_ENTRY_SIZE = 95;
	const uint16_t DELAY_BASE_OFFSET = 0x554;
	const uint16_t DELAY_BASE_OFFSET2 = 0x53c;

	struct qsound_voice {
		uint16_t m_bank = 0;
		int16_t m_addr = 0; // top word is the sample address
		uint16_t m_phase = 0;
		uint16_t m_rate = 0;
		int16_t m_loop_len = 0;
		int16_t m_end_addr = 0;
		int16_t m_volume = 0;
		int16_t m_echo = 0;

		int16_t update(QSound &dsp, int32_t *echo_out);
	};

	struct qsound_adpcm {
		uint16_t m_start_addr = 0;
		uint16_t m_end_addr = 0;
		uint16_t m_bank = 0;
		int16_t m_volume = 0;
		uint16_t m_flag = 0;
		int16_t m_cur_vol = 0;
		int16_t m_step_size = 0;
		uint16_t m_cur_addr = 0;

		int16_t update(QSound &dsp, int16_t curr_sample, int nibble);
	};

	// Q1 Filter
	struct qsound_fir {
		int m_tap_count = 0;    // usually 95
		int m_delay_pos = 0;
		uint16_t m_table_pos = 0;
		int16_t m_taps[95] = { 0 };
		int16_t m_delay_line[95] = { 0 };

		int32_t apply(int16_t input);
	};

	// Delay line
	struct qsound_delay {
		int16_t m_delay = 0;
		int16_t m_volume = 0;
		int16_t m_write_pos = 0;
		int16_t m_read_pos = 0;
		int16_t m_delay_line[51] = { 0 };

		int32_t apply(const int32_t input);
		void update();
	};

	struct qsound_echo {
		uint16_t m_end_pos = 0;

		int16_t m_feedback = 0;
		int16_t m_length = 0;
		int16_t m_last_sample = 0;
		int16_t m_delay_line[1024] = { 0 };
		int16_t m_delay_pos = 0;

		int16_t apply(int32_t input);
	};


	const std::vector<uint8_t>* m_samples = nullptr;
	uint16_t m_dsp_rom[0x1000] = {0};
	bool m_exact = false;

	uint16_t m_data_latch = 0;
	int16_t m_out[2] = {0, 0};

	qsound_voice m_voice[16];
	qsound_adpcm m_adpcm[3];

	uint16_t m_voice_pan[16+3] = {0};
	int16_t m_voice_output[16+3] = {0};

	qsound_echo m_echo;
	qsound_fir m_filter[2];
	qsound_fir m_alt_filter[2];
	qsound_delay m_wet[2];
	qsound_delay m_dry[2];

	uint16_t m_state = 0;
	uint16_t m_next_state = 0;
	uint16_t m_delay_update = 0;
	int m_state_counter = 0;
	uint8_t m_ready_flag = 0;

	uint16_t *m_register_map[256] = {nullptr};

	inline uint16_t read_dsp_rom(uint16_t addr) const { return m_dsp_rom[addr & 0xfff]; }
	void write_data(uint8_t addr, uint16_t data);
	void init_register_map();
	int16_t read_sample(uint16_t bank, uint16_t address);
	void update_sample();
	void state_init();
	void state_refresh_filter_1();
	void state_refresh_filter_2();
	void state_normal_update();
	void useApproximateTables();
};

}  // namespace zen1::cps1
