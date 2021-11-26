#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint8_t byte;
typedef uint16_t word;

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

typedef struct
{
	byte(*read_byte)(void*, word);
	void(*write_byte)(void*, word, byte);
	void* userdata;

	uint32_t screen_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
	uint32_t bg_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
	uint32_t wn_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
	uint32_t colors[4];

	int scanline_counter;
	byte internal_scanline;

} ppu_t;

void ppu_init(ppu_t* const p);
void ppu_step(ppu_t* const p, int elapsed);