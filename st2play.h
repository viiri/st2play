/*
 * st2play - very accurate C port of Scream Tracker 2.xx's replayer,
 *
 * Copyright 2017 Sergei "x0r" Kolzun
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ST2PLAY_H
#define ST2PLAY_H

#define ST2BASEFREQ 36072500  /* 2.21 */
//#define ST2BASEFREQ 35468950 /* 2.3 */

#define FXMULT 0x0a

// LMN can be entered in the editor but don't do anything
#define FX_NONE           0x00
#define FX_SPEED          0x01
#define FX_POSITIONJUMP   0x02
#define FX_PATTERNBREAK   0x03
#define FX_VOLUMESLIDE    0x04
#define FX_PORTAMENTODOWN 0x05
#define FX_PORTAMENTOUP   0x06
#define FX_TONEPORTAMENTO 0x07
#define FX_VIBRATO        0x08
#define FX_TREMOR         0x09
#define FX_ARPEGGIO       0x0a
#define FX_VIBRA_VSLIDE   0x0b
#define FX_TONE_VSLIDE    0x0f

typedef struct st2_channel_s
{
	uint8_t on;
	uint8_t empty;
	uint16_t row;
	uint8_t *pattern_data_offs;
	uint16_t event_note;
	uint8_t event_volume;
	uint16_t event_smp;
	uint16_t event_cmd;
	uint16_t event_infobyte;
	uint16_t last_note;
	uint16_t period_current;
	uint16_t period_target;
	uint16_t vibrato_current;
	uint16_t tremor_counter;
	uint16_t tremor_state;
	uint8_t *smp_name;
	uint8_t *smp_data_ptr;
	uint16_t smp_loop_end;
	uint16_t smp_loop_start;
	uint16_t smp_c2spd;
	uint32_t smp_position;
	uint32_t smp_step;
	uint16_t volume_initial;
	uint16_t volume_current;
	uint16_t volume_meter;
	uint16_t volume_mix;
} st2_channel_t;

typedef struct st2_sample_s {
	uint8_t name[12];
	uint8_t id;
	uint8_t disk;
	uint16_t offset; // !!!
	uint16_t length;
	uint16_t loop_start;
	uint16_t loop_end;
	uint8_t volume;
	uint8_t rsvd2;
	uint16_t c2spd;
	uint32_t rsvd3;
	uint16_t length_par;
	uint8_t *data; // !!!
} st2_sample_t;

typedef struct st2_context_s {
	uint16_t sample_rate;
	uint16_t pattern_current;
	uint8_t change_pattern;
	uint16_t current_tick;
	uint16_t ticks_per_row;
	uint16_t current_frame;
	uint16_t frames_per_tick;
	uint16_t loop_count;
	uint16_t order_first;
	uint16_t order_next;
	uint16_t order_current;
	uint8_t tempo;
	uint8_t global_volume;
	uint8_t play_single_note;
	uint8_t *order_list_ptr;
	uint8_t *pattern_data_ptr;
	st2_channel_t channels[4];
	st2_sample_t samples[32];
} st2_context_t;

void st2_init_tables(void);
st2_context_t *st2_tracker_init(void);
void st2_tracker_start(st2_context_t *ctx, uint16_t sample_rate);
void st2_tracker_destroy(st2_context_t *ctx);
uint16_t st2_get_position(st2_context_t *ctx);
void st2_set_position(st2_context_t *ctx, uint16_t ord);
uint8_t st2_render_sample(st2_context_t *ctx);

#endif
