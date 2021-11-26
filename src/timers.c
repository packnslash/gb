#include "timers.h"

static int timer_freq[4] = { 1024, 16, 64, 256 };

static inline byte rb(timers_t* const t, word addr)
{
	return t->read_byte(t->userdata, addr);
}

static inline void wb(timers_t* const t, word addr, byte val)
{
	t->write_byte(t->userdata, addr, val);
}

void timers_init(timers_t* const t)
{
	t->userdata = NULL;
	t->read_byte = NULL;
	t->write_byte = NULL;

	t->divider_counter = 0;
	t->timer_counter = 0;
	t->div = 0;
}

void timers_step(timers_t* const t, int elapsed)
{
	t->divider_counter += elapsed;

	if (t->divider_counter >= 256)
	{
		t->divider_counter = 0;
		t->div++;
	}

	byte tc = rb(t, 0xFF07);

	if ((tc & 0x4) == 0x4)
	{
		t->timer_counter += elapsed;

		while (t->timer_counter >= timer_freq[tc & 3])
		{
			t->timer_counter = 0;

			byte timer = rb(t, 0xFF05);

			if (timer == 0xFF)
			{
				wb(t, 0xFF0F, rb(t, 0xFF0F) | 0x4);
				wb(t, 0xFF05, rb(t, 0xFF06));
			}
			else
			{
				wb(t, 0xFF05, timer + 1);
			}
		}
	}
}