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
#include <string.h>
#include <stdlib.h>

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

	int i;

	printf("Loading song\n");
	if((fp = fopen(filename, "rb")) == NULL)
	{
		printf("LOAD ERROR!\n");
		return -1;
	}

	fread(&stm.song_name, 1, 20, fp);
	fread(&stm.tracker_name, 1, 9, fp);
	stm.type = fgetc(fp);
	stm.version_major = fgetc(fp);
	stm.version_minor = fgetc(fp);

	if(stm.version_major != 2 && stm.version_minor < 21)
		return -1;

	stm.tempo = fgetc(fp);
	ctx->tempo = stm.tempo;
	stm.patterns = fgetc(fp);
	stm.gvol = fgetc(fp);
	ctx->global_volume = stm.gvol;
	fread(&stm.reserved, 1, 13, fp);

	for (i = 1; i < 32; i++) {
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
		if(ctx->samples[i].loop_end != 0xffff && ctx->samples[i].loop_end > ctx->samples[i].length)
			ctx->samples[i].loop_end = ctx->samples[i].length;
	}

	ctx->order_list_ptr = (uint8_t *)(malloc(128));
	fread(ctx->order_list_ptr, 1, 128, fp);

	ctx->pattern_data_ptr = (uint8_t *)(malloc(0x400 * 64));
	fread(ctx->pattern_data_ptr, 1, 0x400 * stm.patterns, fp);

	for (i = 1; i < 32; i++)
	{
		if (ctx->samples[i].volume && ctx->samples[i].length)
		{
			fseek(fp, ctx->samples[i].offset << 4, SEEK_SET);
			ctx->samples[i].data = (uint8_t *)(malloc(ctx->samples[i].length + 1));
			fread(ctx->samples[i].data, 1, ctx->samples[i].length, fp);
		}
	}

	fclose(fp);
	return 0;
}
