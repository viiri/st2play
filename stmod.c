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
#include <SDL.h>

#include "st2play.h"
#include "stmload.h"

//#define SAMPLING_FREQ  23863
#define SAMPLING_FREQ  48000
#define BUFFER_SAMPLES 16384

static void fill_audio(void *udata, Uint8 *stream, int len)
{
	int i;

	for(i = 0; i < len; i++)
		stream[i] = st2_get_position((st2_context_t *)udata) >> 8 ? 128 : st2_render_sample((st2_context_t *)udata);
}

static int sdl_init(st2_context_t *ctx)
{
	SDL_AudioSpec audiospec;

	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "sdl: can't initialize: %s\n", SDL_GetError());
		return -1;
	}

	audiospec.freq = SAMPLING_FREQ;
	audiospec.format = AUDIO_U8;
	audiospec.channels = 1;
	audiospec.samples = BUFFER_SAMPLES;
	audiospec.callback = fill_audio;
	audiospec.userdata = ctx;

	if (SDL_OpenAudio(&audiospec, NULL) < 0) {
		fprintf(stderr, "%s\n", SDL_GetError());
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	st2_context_t *context;

	if (argc < 2)
	{
		printf("Usage: %s <filename>\n", argv[0]);
		exit(-1);
	}

	context = st2_tracker_init();

	if(stm_load(context, argv[1]))
	    return 1;

	st2_tracker_start(context, SAMPLING_FREQ);
	st2_set_position(context, 0);

	if(sdl_init(context) < 0)
	{
		fprintf(stderr, "%s: can't initialize sound\n", argv[0]);
		return 1;
	}

	SDL_PauseAudio(0);

	while(!(st2_get_position(context) >> 8))
		SDL_Delay(10);

	SDL_CloseAudio();
	st2_tracker_destroy(context);

	return 0;
}

