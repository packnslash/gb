#include <stdint.h>

typedef uint8_t byte;
typedef uint16_t word;

typedef struct
{
	byte(*read_byte)(void*, word);
	void(*write_byte)(void*, word, byte);
	void* userdata;

	int divider_counter;
	int timer_counter;
	byte div;

} timers_t;

void timers_init(timers_t* const t);
void timers_step(timers_t* const t, int elapsed);