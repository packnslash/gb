#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t byte;
typedef uint16_t word;

typedef struct
{
	byte rom[0x200000];
	byte ram[0x8000];

	byte mbc;
	bool rom_enable;
	byte rom_bank;
	byte ram_bank;
	bool ram_enable;
	bool rtc_enable;
	bool latch_zero;
	bool latched;

} cartridge_t;

int cartidge_init(cartridge_t* const cart);
int cartridge_load(cartridge_t* const cart, const char* filename, word addr);
void cartridge_write(cartridge_t* const cart, word addr, byte val);
byte cartridge_read(cartridge_t* const cart, word addr);