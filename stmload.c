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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "st2play.h"
#include "stmload.h"

static uint16_t fgetw(FILE *fp);
static uint32_t fgetl(FILE *fp);

static uint16_t fgetw(FILE *fp)
{
	uint8_t data[2];

	data[0] = fgetc(fp);
	data[1] = fgetc(fp);

	return (data[1] << 8) | data[0];
}

static uint32_t fgetl(FILE *fp)
{
	uint8_t data[4];

	data[0] = fgetc(fp);
	data[1] = fgetc(fp);
	data[2] = fgetc(fp);
	data[3] = fgetc(fp);

	return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}

int stm_load(st2_context_t *ctx, const char *filename)
{
	FILE *fp;
	stm_header_t stm;

	uint8_t code;
	int i, j, result = -1;

	if((fp = fopen(filename, "rb")) == NULL)
	{
		printf("LOAD ERROR!\n");
		goto cleanup;
	}

	fread(&stm.song_name, 1, 20, fp);
	fread(&stm.tracker_name, 1, 9, fp);

	stm.type = fgetc(fp);
	if(stm.type != 1 && stm.type != 2)
	{
		printf("Unknown song type!\n");
		goto cleanup;
	}

	stm.version = 100 * fgetc(fp) + fgetc(fp);
	if(stm.version > 221)
	{
		printf("Unknown version!\n");
		goto cleanup;
	}

	if(stm.version != 200 && stm.version != 210 && stm.version != 220 && stm.version != 221)
	{
		printf("TODO: File version (%i) prior to 2.\n", stm.version);
		goto cleanup;
	}

	stm.tempo = fgetc(fp);
	if(stm.version < 221)
		stm.tempo = (stm.tempo / 10 << 4) + stm.tempo % 10;
	ctx->tempo = stm.tempo;

	stm.patterns = fgetc(fp);

	// TODO: song_init
	ctx->order_list_ptr = (uint8_t *)(malloc(128));
	ctx->pattern_data_ptr = (uint8_t *)(malloc(65536));
	// TODO: song_init

	stm.gvol = fgetc(fp);
	if(stm.version > 210)
		ctx->global_volume = stm.gvol;
	
	fread(&stm.reserved, 1, 13, fp);

	for(i = 1; i < 32; ++i) {
		fread(&ctx->samples[i].name, 1, 12, fp);
		ctx->samples[i].id = fgetc(fp);
		ctx->samples[i].disk = fgetc(fp);
		ctx->samples[i].offset = fgetw(fp);
		ctx->samples[i].length = fgetw(fp);
		ctx->samples[i].loop_start = fgetw(fp);
		ctx->samples[i].loop_end = fgetw(fp);

		if(ctx->samples[i].loop_end == 0)
			ctx->samples[i].loop_end = 0xffff;

		ctx->samples[i].volume = fgetc(fp);
		ctx->samples[i].rsvd2 = fgetc(fp);
		ctx->samples[i].c2spd = fgetw(fp);
		ctx->samples[i].rsvd3 = fgetl(fp);
		ctx->samples[i].length_par = fgetw(fp);

		// NON-ST2: amegas.stm has some samples with loop-point over the sample length.
		if(ctx->samples[i].loop_end != 0xffff && ctx->samples[i].loop_end > ctx->samples[i].length)
			ctx->samples[i].loop_end = ctx->samples[i].length;
	}

	if(stm.version == 200)
		i = 64;
	else
		i = 128;

	fread(ctx->order_list_ptr, 1, i, fp);

	for(i = 0; i < stm.patterns; ++i)
	{
		for(j = 0; j < 1024; ++j)
		{
			code = fgetc(fp);
			switch(code)
			{
				case 0xfb:
					ctx->pattern_data_ptr[(i << 10) + j] = 0; ++j;
					ctx->pattern_data_ptr[(i << 10) + j] = 0; ++j;
					ctx->pattern_data_ptr[(i << 10) + j] = 0; ++j;
					ctx->pattern_data_ptr[(i << 10) + j] = 0;
					break;
				case 0xfd:
					ctx->pattern_data_ptr[(i << 10) + j] = 0xfe; ++j;
					ctx->pattern_data_ptr[(i << 10) + j] = 0x01; ++j;
					ctx->pattern_data_ptr[(i << 10) + j] = 0x80; ++j;
					ctx->pattern_data_ptr[(i << 10) + j] = 0;
					break;
				case 0xfc:
					ctx->pattern_data_ptr[(i << 10) + j] = 0xff; ++j;
					ctx->pattern_data_ptr[(i << 10) + j] = 0x01; ++j;
					ctx->pattern_data_ptr[(i << 10) + j] = 0x80; ++j;
					ctx->pattern_data_ptr[(i << 10) + j] = 0;
					break;
				default:
					ctx->pattern_data_ptr[(i << 10) + j] = code; ++j;
					ctx->pattern_data_ptr[(i << 10) + j] = fgetc(fp); ++j;
					code = ctx->pattern_data_ptr[(i << 10) + j] = fgetc(fp); ++j;
					ctx->pattern_data_ptr[(i << 10) + j] = fgetc(fp);
					if(stm.version < 221 && (code & 0x0f) == 1) {
						code = ctx->pattern_data_ptr[(i << 10) + j];
						ctx->pattern_data_ptr[(i << 10) + j] = (code / 10 << 4) + code % 10;
					}
			}
		}
	}

	// TODO: Add external samples support.
	if(stm.type == 2) {
		for(i = 1; i < 32; ++i)
		{
			if(ctx->samples[i].volume && ctx->samples[i].length)
			{
				fseek(fp, ctx->samples[i].offset << 4, SEEK_SET);
				ctx->samples[i].data = (uint8_t *)(malloc(ctx->samples[i].length + 1));
				fread(ctx->samples[i].data, 1, ctx->samples[i].length, fp);
			}
		}
	}

	result = 0;
cleanup:
	if(fp)
		fclose(fp);

	return result;
}
