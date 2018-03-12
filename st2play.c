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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

#include "st2play.h"

#define ST2BASEFREQ 35468950

static uint16_t tempo_table[18] = { 140, 50, 25, 15, 10, 7, 6, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1 };
static uint16_t period_table[80] = { 17120, 16160, 15240, 14400, 13560, 12800, 12080, 11400, 10760, 10160, 9600, 9070, 0 ,0 ,0 ,0 };
static int16_t lfo_table[65] = {   0,   24,   49,   74,   97,  120,  141,  161,  180,  197,  212,  224,  235,  244,  250,  253,
								 255,  253,  250,  244,  235,  224,  212,  197,  180,  161,  141,  120,   97,   74,   49,   24,
								   0,  -24,  -49,  -74,  -97, -120, -141, -161, -180, -197, -212, -224, -235, -244, -250, -253,
								-255, -253, -250, -244, -235, -224, -212, -197, -180, -161, -141, -120,  -97,  -74,  -49,  -24, 0 };
static uint8_t volume_table[65][256];

static void generate_period_table(void);
static void generate_volume_table(void);

static void set_tempo(st2_context_t *ctx, uint8_t tempo);
static void update_frequency(st2_context_t *ctx, size_t chn);
static void cmd_row(st2_context_t *ctx, size_t chn);
static void cmd_tick(st2_context_t *ctx, size_t chn);
static void trigger_note(st2_context_t *ctx, size_t chn);
static void process_row(st2_context_t *ctx, size_t chn);
static void change_pattern(st2_context_t *ctx);
static void process_tick(st2_context_t *ctx);

static void generate_period_table(void)
{
	size_t i;
	static uint8_t period_table_initialized = 0;

	if(!period_table_initialized) {
		for(i = 0; i < 64; ++i)
			period_table[i + 16] = period_table[i] >> 1;
		period_table_initialized = 1;
	}
}

static void generate_volume_table(void)
{
	size_t i, j;
	static uint8_t volume_table_initialized = 0;

	if(!volume_table_initialized) {
		for(i = 0; i < 65; ++i)
			for(j = 0; j < 256; ++j)
				volume_table[i][j] = (uint8_t)((i * (int8_t)j) / 256);
		volume_table_initialized = 1;
	}
}

static void set_tempo(st2_context_t *ctx, uint8_t tempo)
{
	ctx->tempo = tempo;
	ctx->ticks_per_row = tempo >> 4;
	ctx->frames_per_tick = ctx->sample_rate / (50 - ((tempo_table[ctx->ticks_per_row] * (tempo & 0x0f)) >> 4));
}

static void update_frequency(st2_context_t *ctx, size_t chn)
{
	uint32_t temp, step = 0;
	st2_channel_t *ch = &ctx->channels[chn];

	if(ch->period_current >= 551) {
		temp = ST2BASEFREQ / ch->period_current;
		step = ((temp / ctx->sample_rate) & 0xffff) << 16;
		step |= (((temp % ctx->sample_rate) << 16) / ctx->sample_rate) & 0xffff;
	}

	ch->smp_step = step;
}

static void cmd_row(st2_context_t *ctx, size_t chn)
{
	st2_channel_t *ch = &ctx->channels[chn];

	switch(ch->event_cmd)
	{
		case FX_SPEED:
			if(ch->event_infobyte)
				set_tempo(ctx, ch->event_infobyte);
			break;
		case FX_POSITIONJUMP:
			ctx->order_next = ch->event_infobyte;
			break;
		case FX_PATTERNBREAK:
			ctx->change_pattern = 1;
			break;
	}
}

static void cmd_tick(st2_context_t *ctx, size_t chn)
{
	uint8_t octa, note, note_add;
	st2_channel_t *ch = &ctx->channels[chn];

	switch(ch->event_cmd)
	{
		case FX_ARPEGGIO:
			if((ctx->current_tick % 3) == 1)
				note_add = 0;
			else if((ctx->current_tick % 3) == 2)
				note_add = ch->event_infobyte >> 4;
			else
				note_add = ch->event_infobyte & 0x0f;

			octa =  ch->last_note & 0xf0;
			note = (ch->last_note & 0x0f) + note_add;

			if(note >= 11) {
				note -= 12;
				octa += 16;
			}

			ch->period_current = period_table[octa | note] * 8192 / (ch->smp_c2spd ? ch->smp_c2spd : 8192);
			ch->period_target = ch->period_current;

			update_frequency(ctx, chn);
			break;
		case FX_TREMOR:
			if(ch->tremor_counter == 0) {
				if(ch->tremor_state == 1) {
					ch->tremor_state = 0;
					ch->volume_current = 0;
					ch->tremor_counter = ch->event_infobyte & 0x0f;
				} else {
					ch->tremor_state = 1;
					ch->volume_current = ch->volume_initial;
					ch->tremor_counter = ch->event_infobyte >> 4;
				}
			} else {
				ch->tremor_counter--;
			}
			break;
		default:
			ch->tremor_counter = 0;
			ch->tremor_state = 1;
			switch(ch->event_cmd)
			{
				case FX_TONEPORTAMENTO:
					// TODO: Ugly assembler legacy
					if(ch->event_infobyte == 0)
						fx_toneportamento: ch->event_infobyte = ch->last_infobyte1;
					ch->last_infobyte1 = ch->event_infobyte;

					if(ch->period_current != ch->period_target) {
						if((int16_t)ch->period_current > (int16_t)ch->period_target) {
							ch->period_current -= FXMULT * ch->event_infobyte;
							if((int16_t)ch->period_current < (int16_t)ch->period_target)
								ch->period_current = ch->period_target;
						} else {
							ch->period_current += FXMULT * ch->event_infobyte;
							if((int16_t)ch->period_current > (int16_t)ch->period_target)
								ch->period_current = ch->period_target;
						}
						update_frequency(ctx, chn);
					}
					break;
				case FX_VIBRATO:
					// TODO: Ugly assembler legacy
					if(ch->event_infobyte == 0)
						fx_vibrato: ch->event_infobyte = ch->last_infobyte2;
					ch->last_infobyte2 = ch->event_infobyte;

					ch->period_current = (FXMULT * ((lfo_table[ch->vibrato_current >> 1] * (ch->event_infobyte & 0x0f)) >> 7)) + ch->period_target;
					update_frequency(ctx, chn);
					ch->vibrato_current = (ch->vibrato_current + ((ch->event_infobyte >> 4) << 1)) & 0x7e;
					break;
				default:
					ch->vibrato_current = 0;
					switch(ch->event_cmd)
					{
						case FX_PORTAMENTODOWN:
							ch->period_current += FXMULT * ch->event_infobyte;
							update_frequency(ctx, chn);
							break;
						case FX_PORTAMENTOUP:
							ch->period_current -= FXMULT * ch->event_infobyte;
							update_frequency(ctx, chn);
							break;
						case FX_VIBRA_VSLIDE:
						case FX_TONE_VSLIDE:
							goto fx_volumeslide;
						case FX_VOLUMESLIDE:
						default:
							if(ch->period_current != ch->period_target) {
								ch->period_current = ch->period_target;
								update_frequency(ctx, chn);
							}

							if(ch->event_cmd != FX_VOLUMESLIDE)
								break;
						fx_volumeslide:
							if(ch->event_infobyte & 0x0f) {
								ch->volume_current -= ch->event_infobyte & 0x0f;
								if((int16_t)ch->volume_current <= -1)
									ch->volume_current = 0;
							} else {
								ch->volume_current += ch->event_infobyte >> 4;
								if(ch->volume_current >= 65)
									ch->volume_current = 64;
							}

							if(ch->event_cmd == FX_TONE_VSLIDE)
								goto fx_toneportamento;
							if(ch->event_cmd == FX_VIBRA_VSLIDE)
								goto fx_vibrato;
					}
			}
	}
}

static void trigger_note(st2_context_t *ctx, size_t chn)
{
	st2_channel_t *ch = &ctx->channels[chn];

	if(ch->event_volume != 65) {
		ch->volume_current = ch->event_volume;
		ch->volume_initial = ch->volume_current;
	}

	if(ch->event_cmd == FX_TONEPORTAMENTO) {
		if(ch->event_note != 255)
			ch->period_target = period_table[ch->event_note] * 8192 / (ch->smp_c2spd ? ch->smp_c2spd : 8192);
		return;
	}

	if(ch->event_smp != 0) {
		ch->smp_name = ctx->samples[ch->event_smp].name;
		if(ch->event_volume == 65) {
			ch->volume_current = ctx->samples[ch->event_smp].volume & 0xff;
			ch->volume_initial = ch->volume_current;
		}

		ch->smp_c2spd = ctx->samples[ch->event_smp].c2spd;
		if(ctx->samples[ch->event_smp].data == NULL) {
			ch->event_note = 254;
			ch->smp_data_ptr = NULL;
		} else {
			ch->smp_data_ptr = ctx->samples[ch->event_smp].data;
		}

		if(ctx->samples[ch->event_smp].loop_end != 0xffff) {
			ch->smp_loop_end = ctx->samples[ch->event_smp].loop_end;
			ch->smp_loop_start = ctx->samples[ch->event_smp].loop_start;
		} else {
			ch->smp_loop_end = ctx->samples[ch->event_smp].length;
			ch->smp_loop_start = 0xffff;
		}
	}

	if(ch->event_note == 254) {
		ch->smp_position = 0;
		ch->smp_loop_end = 0;
		ch->smp_loop_start = 0xffff;
	} else {
		if(ch->event_note != 255) {
			ch->last_note = ch->event_note;
			ch->volume_meter = ch->volume_current >> 1;
			ch->period_current = period_table[ch->event_note] * 8192 / (ch->smp_c2spd ? ch->smp_c2spd : 8192);
			ch->period_target = ch->period_current;
			update_frequency(ctx, chn);
			ch->smp_position = 0;
		}
	}

	cmd_row(ctx, chn);
}

static void process_row(st2_context_t *ctx, size_t chn)
{
	uint8_t *pd;
	st2_channel_t *ch = &ctx->channels[chn];

	ch->row++;
	if(ch->row >= 64)
		ctx->change_pattern = 1;

	if(ch->on) {
		pd = ch->pattern_data_offs;

		ch->event_note =     *(pd);
		ch->event_smp =      *(pd + 1) >> 3;
		ch->event_volume =  (*(pd + 1) & 7) | ((*(pd + 2) >> 1) & 0x78);
		ch->event_cmd =      *(pd + 2) & 0x0f;
		ch->event_infobyte = *(pd + 3);

		ch->pattern_data_offs += 0x10;

		trigger_note(ctx, chn);

		if(ch->event_cmd == FX_TREMOR)
			cmd_tick(ctx, chn);
	}
}

static void change_pattern(st2_context_t *ctx)
{
	size_t i, j;

	if(ctx->order_list_ptr[ctx->order_next] == 98 || ctx->order_list_ptr[ctx->order_next] == 99) {
		ctx->order_next = ctx->order_list_ptr[ctx->order_next] == 99 ? ctx->order_first : 0;
		ctx->loop_count++;
	}

	ctx->pattern_current = ctx->order_list_ptr[ctx->order_next];
	// TODO: Subsong support, possible in loader.
	// Uncomment to break song looping:
	// ctx->order_list_ptr[ctx->order_next] = 99;
	ctx->order_current = ctx->order_next++;

	for(i = 0, j = 0; i < 4; ++i, j += 4)
	{
		ctx->channels[i].pattern_data_offs = ctx->pattern_data_ptr + (0x400 * ctx->pattern_current) + j;
		ctx->channels[i].row = 0;
	}
}

static void process_tick(st2_context_t *ctx)
{
	size_t i;

	if(ctx->current_tick != 0) {
		ctx->current_tick--;
		for(i = 0; i < 4; ++i)
			cmd_tick(ctx, i);
	} else {
		if(!ctx->play_single_note) {
			if(ctx->change_pattern) {
				ctx->change_pattern = 0;
				change_pattern(ctx);
			}

			for(i = 0; i < 4; ++i)
				process_row(ctx, i);

			ctx->current_tick = ctx->ticks_per_row != 0 ? ctx->ticks_per_row - 1 : 0;
		}
	}

	for(i = 0; i < 4; ++i)
		ctx->channels[i].volume_mix = (ctx->channels[i].volume_current * ctx->global_volume) >> 6;
}

uint8_t st2_render_sample(st2_context_t *ctx)
{
	size_t i;
	uint8_t mix = 0;
	st2_channel_t *ch;

	for(i = 0; i < 4; ++i)
	{
		ch = &ctx->channels[i];
		if((ch->smp_position >> 16) >= ch->smp_loop_end) {
			if(ch->smp_loop_start != 0xffff) {
				ch->smp_position = (ch->smp_loop_start << 16) | (ch->smp_position & 0xffff);
			} else {
				ch->empty = 1;
				continue;
			}
		}

		ch->smp_position += ch->smp_step;

		if(ch->smp_data_ptr != NULL && (ch->smp_position >> 16) < ch->smp_loop_end && ch->volume_mix < 65)
			mix += volume_table[ch->volume_mix][ch->smp_data_ptr[ch->smp_position >> 16]];
	}

	if(ctx->current_frame == 1) {
		ctx->current_frame = ctx->frames_per_tick;
		process_tick(ctx);
	} else {
		ctx->current_frame--;
	}

	return mix + 128;
}

st2_context_t *st2_tracker_init(void)
{
	size_t i;
	st2_context_t *ctx;

	ctx = (st2_context_t *)(malloc(sizeof(st2_context_t)));
	if(ctx == NULL)
		return NULL;

	memset(ctx, 0, sizeof(st2_context_t));

	generate_volume_table();
	generate_period_table();

	ctx->tempo = 0x60;
	ctx->global_volume = 64;
	ctx->sample_rate = 15909;
	ctx->frames_per_tick = ctx->current_frame = 1;

	for(i = 0; i < 32; ++i)
	{
		ctx->samples[i].length = 0;
		ctx->samples[i].loop_start = 0;
		ctx->samples[i].loop_end = 0xffff;
		ctx->samples[i].volume = 0;
		ctx->samples[i].c2spd = 8192;
	}

	return ctx;
}

void st2_tracker_start(st2_context_t *ctx, uint16_t sample_rate)
{
	size_t i;

	ctx->sample_rate = sample_rate ? sample_rate : 15909;

	for(i = 0; i < 4; ++i)
	{
		ctx->channels[i].on = 1;
		ctx->channels[i].smp_loop_start = 0xffff;
	}

	set_tempo(ctx, ctx->tempo);
	ctx->current_frame = ctx->frames_per_tick;
	change_pattern(ctx);
}

uint16_t st2_get_position(st2_context_t *ctx)
{
	return (ctx->loop_count << 8) | (ctx->order_current & 0xff);
}

void st2_set_position(st2_context_t *ctx, uint16_t ord)
{
	ctx->order_next = ctx->order_first = ord;
	change_pattern(ctx);
}

void st2_tracker_destroy(st2_context_t *ctx)
{
	size_t i;

	if(ctx != NULL) {
		if(ctx->order_list_ptr)
			free(ctx->order_list_ptr);

		if(ctx->pattern_data_ptr)
			free(ctx->pattern_data_ptr);

		for(i = 0; i < 32; ++i)
			if(ctx->samples[i].data)
				free(ctx->samples[i].data);

		free(ctx);
	}
}
