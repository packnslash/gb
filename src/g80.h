#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef uint8_t byte;
typedef uint16_t word;

#define FRAMERATE 59.7275
#define CLOCKSPEED 4194304

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

typedef struct g80 {

	byte(*read_byte)(void*, word);
	void(*write_byte)(void*, word, byte);
	void* userdata;

	byte a, b, c, d, e, h, l;
	word pc, sp;

	unsigned long cyc;

	bool zf : 1, nf : 1, hf : 1, cf : 1;
	bool halted : 1;
	bool iff : 1;
	int interrupt_delay;
	
	int scanline_counter;
	int divider_counter;
	int timer_counter;

	uint32_t screen_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
	uint32_t bg_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
	uint32_t wn_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

	uint32_t colors[4];

	byte internal_scanline;
	byte div;

	bool bp;

} g80;

void g80_init(g80* const c);
int g80_step(g80* const c);
void ppu_step(g80* const c, int elapsed);
void timer_step(g80* const c, int elapsed);
void process_interrupts(g80* const c);
