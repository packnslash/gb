#include "ppu.h"

static inline byte rb(ppu_t* const p, word addr)
{
	return p->read_byte(p->userdata, addr);
}

static inline void wb(ppu_t* const p, word addr, byte val)
{
	p->write_byte(p->userdata, addr, val);
}

static inline void draw_scanline(ppu_t* const p, byte scanline)
{
	byte control = rb(p, 0xFF40);

	word bg_tile_map = ((control & 0x08) == 0x08) ? 0x9C00 : 0x9800;
	word wn_tile_map = ((control & 0x40) == 0x40) ? 0x9C00 : 0x9800;
	word tile_data = ((control & 0x10) == 0x10) ? 0x8000 : 0x8800;

	byte scroll_x = rb(p, 0xFF43);
	byte scroll_y = rb(p, 0xFF42);
	byte window_y = rb(p, 0xFF4A);
	byte window_x = rb(p, 0xFF4B) - 7;
	byte palette = rb(p, 0xFF47);

	bool window = ((control & 0x20) == 0x20);

	byte bg_y = scanline + scroll_y;
	byte wn_y = p->internal_scanline - window_y;

	word bg_tile_row = (bg_y / 8) * 32;
	word wn_tile_row = (wn_y / 8) * 32;

	for (int px = 0; px < 160; px++)
	{
		byte bg_x = px + scroll_x;
		byte wn_x = px - window_x;

		word bg_tile_col = bg_x / 8;
		word wn_tile_col = wn_x / 8;

		word bg_tile_addr = bg_tile_map + bg_tile_row + bg_tile_col;
		word wn_tile_addr = wn_tile_map + wn_tile_row + wn_tile_col;

		int16_t bg_tile_index = ((control & 0x10) == 0x10) ? (byte)rb(p, bg_tile_addr) : (int8_t)rb(p, bg_tile_addr);
		int16_t wn_tile_index = ((control & 0x10) == 0x10) ? (byte)rb(p, wn_tile_addr) : (int8_t)rb(p, wn_tile_addr);

		word bg_tile_loc = ((control & 0x10) == 0x10) ? tile_data + bg_tile_index * 16 : tile_data + (bg_tile_index + 128) * 16;
		word wn_tile_loc = ((control & 0x10) == 0x10) ? tile_data + wn_tile_index * 16 : tile_data + (wn_tile_index + 128) * 16;

		byte bg_line = (bg_y % 8) * 2;
		byte wn_line = (wn_y % 8) * 2;

		byte bg_lo = rb(p, bg_tile_loc + bg_line);
		byte bg_hi = rb(p, bg_tile_loc + bg_line + 1);

		byte wn_lo = rb(p, wn_tile_loc + wn_line);
		byte wn_hi = rb(p, wn_tile_loc + wn_line + 1);

		byte bg_n = 7 - (bg_x % 8);
		byte wn_n = 7 - (wn_x % 8);

		byte bg_color = ((bg_lo & (1 << bg_n)) != 0) | (((bg_hi & (1 << bg_n)) != 0) << 1);
		byte wn_color = ((wn_lo & (1 << wn_n)) != 0) | (((wn_hi & (1 << wn_n)) != 0) << 1);

		p->bg_buffer[px + scanline * SCREEN_WIDTH] = p->colors[(palette >> (bg_color * 2)) & 3];
		p->wn_buffer[px + scanline * SCREEN_WIDTH] = p->colors[(palette >> (wn_color * 2)) & 3];

		if ((control & 0x1) == 0x1)
		{
			p->screen_buffer[px + scanline * SCREEN_WIDTH] = (window && scanline >= window_y && px >= window_x) ? p->wn_buffer[px + scanline * SCREEN_WIDTH] : p->bg_buffer[px + scanline * SCREEN_WIDTH];
		}
		else p->screen_buffer[px + scanline * SCREEN_WIDTH] = p->colors[0];
	}

	if ((control & 0x2) == 0x2)
	{
		word oam_addr = 0xFE00;
		byte height = ((control & 0x4) == 0x4) ? 16 : 8;
		byte count = 0;

		byte xPositions[10];

		for (int s = 0; s < 40; s++)
		{
			if (count >= 10) break;

			byte yPos = rb(p, oam_addr++) - 16;
			byte xPos = rb(p, oam_addr++) - 8;
			byte tile_index = rb(p, oam_addr++);
			byte attributes = rb(p, oam_addr++);

			palette = ((attributes & 0x10) == 0x10) ? rb(p, 0xFF49) : rb(p, 0xFF48);

			if (scanline < yPos || scanline >= yPos + height) continue;

			bool yFlip = (attributes & 0x40) == 0x40;
			bool xFlip = (attributes & 0x20) == 0x20;

			byte line = (yFlip) ? (height - scanline + yPos - 1) * 2 : (scanline - yPos) * 2;

			if (height == 16)
			{
				tile_index &= 0xFE;
			}

			byte lo = rb(p, 0x8000 + tile_index * 16 + line);
			byte hi = rb(p, 0x8000 + tile_index * 16 + line + 1);

			bool priority = true;
			for (int i = 0; i < count; i++)
			{
				if (xPos == xPositions[i])
				{
					priority = false;
					break;
				}
			}

			if (priority)	// if priority and color 0, THIS SHOULD draw the sprite underneath
			{
				for (int px = 0; px < 8; px++)
				{
					byte n = (xFlip) ? px : 7 - px;
					byte color = ((lo & (1 << n)) != 0) | (((hi & (1 << n)) != 0) << 1);

					if (color == 0) continue;

					if (xPos + px < 0 || xPos + px >= 160) continue;

					if ((attributes & 0x80) == 0 || p->screen_buffer[xPos + px + scanline * SCREEN_WIDTH] == p->colors[rb(p, 0xFF47) & 3])
					{
						p->screen_buffer[xPos + px + scanline * SCREEN_WIDTH] = p->colors[palette >> (color * 2) & 3];
					}
				}
			}

			xPositions[count] = xPos;

			count++;
		}
	}
}

static inline void update_lcdstatus(ppu_t* const p)
{
	byte status = rb(p, 0xFF41);
	byte scanline = rb(p, 0xFF44);
	byte mode = status & 3;

	byte next_mode = 0;
	bool need_int = false;

	if (scanline >= 144)
	{
		next_mode = 1;
		if (next_mode != mode)
		{
			p->internal_scanline = 0;
			wb(p, 0xFF0F, rb(p, 0xFF0F) | 0x1);
		}

		need_int = ((status & 0x10) == 0x10);
	}
	else
	{
		if (p->scanline_counter >= 456 - 80)
		{
			next_mode = 2;
			need_int = ((status & 0x20) == 0x20);
		}
		else if (p->scanline_counter >= 456 - 80 - 172)
		{
			next_mode = 3;
		}
		else
		{
			next_mode = 0;

			if (next_mode != mode)
			{
				if (scanline < 144)
				{
					;					draw_scanline(p, scanline);
				}
			}

			need_int = ((status & 0x08) == 0x08);
		}
	}

	if (need_int && (mode != next_mode))
	{
		wb(p, 0xFF0F, rb(p, 0xFF0F) | 0x2);
	}

	if (scanline == rb(p, 0xFF45))
	{
		if ((status & 0x04) == 0)
		{
			status |= 0x4;
			if ((status & 0x40) == 0x40)
			{
				wb(p, 0xFF0F, rb(p, 0xFF0F) | 0x2);
			}
		}
	}
	else status &= ~0x4;

	status &= ~3;
	status |= next_mode;

	wb(p, 0xFF41, status);
}

void ppu_init(ppu_t* const p)
{
	p->write_byte = NULL;
	p->read_byte = NULL;
	p->userdata = NULL;

	memset(p->screen_buffer, 0, sizeof(p->screen_buffer));
	memset(p->bg_buffer, 0, sizeof(p->bg_buffer));
	memset(p->wn_buffer, 0, sizeof(p->wn_buffer));

	p->colors[0] = 0xff58d0d0;
	p->colors[1] = 0xff40a8a0;
	p->colors[2] = 0xff288070;
	p->colors[3] = 0xff105040;

	p->scanline_counter = 456;
	p->internal_scanline = 0;
}

void ppu_step(ppu_t* const p, int elapsed)
{
	p->scanline_counter -= elapsed;

	if (p->scanline_counter <= 0)
	{
		byte scanline = rb(p, 0xFF44);

		wb(p, 0xFF44, ++scanline);

		if (scanline > 153)
		{
			wb(p, 0xFF44, 0);
		}

		p->scanline_counter = 456;

		if ((rb(p, 0xFF40) & 0x20) == 0x20 && rb(p, 0xFF4A) >= 0 && rb(p, 0xFF4A) < 144 && rb(p, 0xFF4B) >= 0 && rb(p, 0xFF4B) < 167)
		{
			if (p->internal_scanline == 0)
			{
				p->internal_scanline = scanline;
			}
			else if (p->internal_scanline != scanline)
			{
				p->internal_scanline++;
			}
			else
			{
				p->internal_scanline = scanline;
			}


			if (p->internal_scanline > 153) p->internal_scanline = 0;
		}
	}

	update_lcdstatus(p);
}