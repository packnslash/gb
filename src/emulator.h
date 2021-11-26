#pragma once

#include <stdio.h>
#include <string.h>

#include "cartridge.h"
#include "g80.h"
#include "ppu.h"
#include "timers.h"

typedef struct gameboy {

	cartridge_t cartridge;

	g80 cpu;

	ppu_t ppu;

	timers_t timers;

	byte memory[0x10000];

	byte joypad; // Down, Up, Left, Right, Start, Select, B, A

} gameboy;

void gameboy_init(gameboy* const gb);