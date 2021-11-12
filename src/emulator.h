#pragma once

#include <stdio.h>
#include <string.h>

#include "g80.h"

typedef struct gameboy {

	g80 cpu;
	byte bootrom[0x100];
	byte cartridge[0x200000];
	byte memory[0x10000];
	byte ram[0x8000];

	byte mbc;
	bool rom_enable;
	byte rom_bank;
	byte ram_bank;
	bool ram_enable;
	bool rtc_enable;
	bool latch_zero;
	bool latched;

	byte joypad; // Down, Up, Left, Right, Start, Select, B, A

} gameboy;

void gameboy_init(gameboy* const gb);
int gameboy_load_boot_rom(gameboy* const gb, const char* filename);
int gameboy_load_rom(gameboy* const gb, const char* filename, word addr);