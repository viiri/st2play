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

#ifndef STMLOAD_H
#define STMLOAD_H

typedef struct stm_header_s {
	uint8_t song_name[20];	/* ASCIIZ song name */
	uint8_t tracker_name[9];	/* '!Scream!\x1a' */
	uint8_t type;		/* 1=song, 2=module */
	uint8_t version_major;	/* Major version number */
	uint8_t version_minor;	/* Minor version number */
	uint8_t tempo;		/* Playback tempo */
	uint8_t patterns;		/* Number of patterns */
	uint8_t gvol;		/* Global volume */
	uint8_t reserved[13];	/* Reserved */
} stm_header_t;

int stm_load(st2_context_t *ctx, const char *filename);

#endif
