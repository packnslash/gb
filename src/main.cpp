#include <map>
#include <string>
#include <SDL.h>

extern "C"
{
	#include "emulator.h"
}

// #define DEBUG
#define SCALE 1

#ifdef DEBUG

#define SCREEN_WIDTH	512
#define SCREEN_HEIGHT	512

static uint32_t font[128 * 48];
static uint32_t screen[SCREEN_WIDTH * SCREEN_HEIGHT];
static uint32_t colors[4] = { 0xFF502C33, 0xFF8F8746, 0xFF44E394, 0xFFE4F3E2 };
static std::map<word, std::string> disassembly;

#endif

static bool should_quit = false;
static bool paused = false;

static uint32_t current_time = 0;
static uint32_t last_time = 0;
static uint32_t dt = 0;

static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static SDL_Event e;

static gameboy gb;

#ifdef DEBUG

std::string hex(uint32_t n, uint8_t d)
{
	std::string s(d, '0');
	for (int i = d - 1; i >= 0; i--, n >>= 4)
		s[i] = "0123456789ABCDEF"[n & 0xF];
	return s;
};

static inline void clear(uint32_t col)
{
	for (int i = 0; i < SCREEN_HEIGHT * SCREEN_WIDTH; i++)
	{
		screen[i] = col;
	}
}

static inline void draw(int x, int y, uint32_t col, bool alpha)
{
	if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;

	if (alpha)
	{
		if ((col >> 24) == 0xFF)
		{
			screen[x + y * SCREEN_WIDTH] = col;
		}
	}
	else
	{
		screen[x + y * SCREEN_WIDTH] = col;
	}
}

static inline void draw_string(int x, int y, std::string text, uint32_t col, uint32_t scale)
{
    int32_t sx = 0;
    int32_t sy = 0;
    for (auto c : text)
    {
        if (c == '\n')
        {
            sx = 0; sy += 8 * scale;
        }
        else
        {
            int32_t ox = (c - 32) % 16;
            int32_t oy = (c - 32) / 16;

            if (scale > 1)
            {
                for (uint32_t i = 0; i < 8; i++)
                    for (uint32_t j = 0; j < 8; j++)
						if ((font[(i + ox * 8) + (j + oy * 8) * 128] & 0xFF000000) > 0)
                            for (uint32_t is = 0; is < scale; is++)
                                for (uint32_t js = 0; js < scale; js++)
                                    draw(x + sx + (i * scale) + is, y + sy + (j * scale) + js, col, false);
            }
            else
            {
                for (uint32_t i = 0; i < 8; i++)
                    for (uint32_t j = 0; j < 8; j++)
                        if ((font[(i + ox * 8) + (j + oy * 8) * 128] & 0xFF000000) > 0)
							draw(x + sx + i, y + sy + j, col, false);
            }
            sx += 8 * scale;
        }
    }
}

static inline void draw_cpu(int x, int y)
{
	draw_string(x, y, "PC: " + hex(gb.cpu.pc, 4), colors[3], 1);
	draw_string(x, y + 10, " A: " + hex(gb.cpu.a, 4), colors[3], 1);
	draw_string(x, y + 20, "BC: " + hex(gb.cpu.c | (gb.cpu.b << 8 ), 4), colors[3], 1);
	draw_string(x, y + 30, "DE: " + hex(gb.cpu.e | (gb.cpu.d << 8 ), 4), colors[3], 1);
	draw_string(x, y + 40, "HL: " + hex(gb.cpu.l | (gb.cpu.h << 8 ), 4), colors[3], 1);
	draw_string(x, y + 50, "SP: " + hex(gb.cpu.sp, 4), colors[3], 1);
	
	draw_string(x     , y + 70, "Z", (gb.cpu.zf == 0) ? colors[3] : colors[2], 1);
	draw_string(x + 20, y + 70, "N", (gb.cpu.nf == 0) ? colors[3] : colors[2], 1);
	draw_string(x + 40, y + 70, "H", (gb.cpu.hf == 0) ? colors[3] : colors[2], 1);
	draw_string(x + 60, y + 70, "C", (gb.cpu.cf == 0) ? colors[3] : colors[2], 1);
}

static inline void draw_code(int x, int y, int nLines)
{
	auto it_a = disassembly.find(gb.cpu.pc);
	int nLineY = (nLines >> 1) * 10 + y;
	if (it_a != disassembly.end())
	{
		draw_string(x, nLineY, (*it_a).second, colors[1], 1);
		while (nLineY < (nLines * 10) + y)
		{
			nLineY += 10;
			if (++it_a != disassembly.end())
			{
				draw_string(x, nLineY, (*it_a).second, colors[3], 1);
			}
		}
	}
	
	it_a = disassembly.find(gb.cpu.pc);
	nLineY = (nLines >> 1) * 10 + y;
	if (it_a != disassembly.end())
	{
		while (nLineY > y)
		{
			nLineY -= 10;
			if (--it_a != disassembly.end())
			{
				draw_string(x, nLineY, (*it_a).second, colors[3], 1);
			}
		}
	}
}

static inline void draw_tiles(int x, int y)
{
	word addr = 0x8000;

		for (int ty = 0; ty < 12; ty++)
		{
			for (int tx = 0; tx < 32; tx++)
			{
				for (int py = 0; py < 8; py++)
				{
					byte lo = gb.memory[addr++];
					byte hi = gb.memory[addr++];

					for (int px = 0; px < 8; px++)
					{
						byte b = 1 << (7 - px);

						byte color = ((lo & b) != 0) | ((hi & b) != 0) << 1;

						screen[(x + px + tx * 8) + (y + py + ty * 8) * SCREEN_WIDTH] = gb.ppu.colors[color];
					}
				}
			}
		}
}

static inline void draw_tile(int index, int x, int y)
{
	word start_addr = ((gb.memory[0xFF40] & (1 << 4)) == (1 << 4)) ? 0x8000 : 0x8800;
	word addr = start_addr + index * 16;

	if ((gb.memory[0xFF40] & (1 << 4)) == 0)
	{
		byte direction = (int8_t)index + 128;
		addr = start_addr + direction * 16;
	}

	for (int py = 0; py < 8; py++)
	{
		byte lo = gb.memory[addr++];
		byte hi = gb.memory[addr++];

		for (int px = 0; px < 8; px++)
		{
			byte b = (7 - px);

			byte color = ((lo & (1 << b)) != 0) | (((hi & (1 << b)) != 0) << 1);
			byte palette = gb.memory[0xFF47];

			screen[(x + px) + (y + py) * SCREEN_WIDTH] = gb.ppu.colors[(palette >> (color * 2)) & 3];
		}
	}
}

static inline void draw_tilemap(int x, int y, bool draw_scroll_window)
{
	word addr = 0x9800;
	for (int ty = 0; ty < 32; ty++)
	{
		for (int tx = 0; tx < 32; tx++)
		{
			byte index = gb.memory[addr++];
			draw_tile(index, x + tx * 8, y + ty * 8);
		}
	}

	addr = 0x9C00;
	for (int ty = 0; ty < 32; ty++)
	{
		for (int tx = 0; tx < 32; tx++)
		{
			byte index = gb.memory[addr++];
			draw_tile(index, 256 + x + tx * 8, y + ty * 8);
		}
	}

	if (draw_scroll_window)
	{
		int sx = gb.memory[0xFF43];
		int sy = gb.memory[0xFF42];

		if ((gb.memory[0xFF40] & 0x10) == 0x10)
		{
			x += 256;
		}

		for (int px = 0; px < 160; px++)
		{
			int xx = x + sx + px;
			int yy = y + sy + 143;

			if (yy < y) y += 256;
			if (yy >= y + 256) yy -= 256;

			if (xx < x) x += 256;
			if (xx >= x + 256) xx -= 256;

			draw(xx, y + sy, colors[1], false);
			draw(xx, yy, colors[1], false);
		}

		for (int py = 0; py < 142; py++)
		{
			int xx = x + sx + 159;
			int yy = y + sy + py + 1;

			if (xx < x) x += 256;
			if (xx >= x + 256) xx -= 256;

			if (yy < y) y += 256;
			if (yy >= y + 256) yy -= 256;

			draw(x + sx, yy, colors[1], false);
			draw(xx, yy, colors[1], false);
		}
	}
}

static inline void draw_screen(int x, int y, bool draw_scanline, bool draw_window)
{
	for (int py = 0; py < 144; py++)
	{
		for (int px = 0; px < 160; px++)
		{
			draw(x + px + 170, y + py, gb.ppu.bg_buffer[px + py * 160], false);
			draw(x + px + 170 + 170, y + py, gb.ppu.wn_buffer[px + py * 160], false);

			draw(x + px, y + py, gb.ppu.screen_buffer[px + py * 160], false);
		}
	}

	if (draw_scanline)
	{
		for (int i = 0; i < 160; i++)
		{
			draw(x + i, y + gb.memory[0xFF44], colors[2], false);
		}
	}

	if (draw_window)
	{
		int wx = gb.memory[0xFF4B] - 7;
		int wy = gb.memory[0xFF4A];

		for (int px = 0; px < 160; px++)
		{
			int xx = x + wx + px;
			int yy = y + wy + 143;

			//if (yy < y) y += 256;
			//if (yy >= y + 256) yy -= 256;
			//
			//if (xx < x) x += 256;
			//if (xx >= x + 256) xx -= 256;

			if (xx < 0 || yy < 0 || xx >= 160 || yy >= 144) continue;

			draw(xx, y + wy, colors[1], false);
			draw(xx, yy, colors[1], false);
		}

		for (int py = 0; py < 142; py++)
		{
			int xx = x + wx + 159;
			int yy = y + wy + py + 1;

			//if (xx < x) x += 256;
			//if (xx >= x + 256) xx -= 256;
			//
			//if (yy < y) y += 256;
			//if (yy >= y + 256) yy -= 256;

			if (xx < 0 || yy < 0 || xx >= 160 || yy >= 144) continue;

			draw(x + wx, yy, colors[1], false);
			draw(xx, yy, colors[1], false);
		}
	}
}

static inline void draw_lcd(int x, int y)
{
	draw_string(x      , y, "E", ((gb.memory[0xFF40] & 0x80) == 0x80) ? colors[2] : colors[1], 1);
	draw_string(x +  15, y, "T", ((gb.memory[0xFF40] & 0x40) == 0x40) ? colors[2] : colors[1], 1);
	draw_string(x +  30, y, "W", ((gb.memory[0xFF40] & 0x20) == 0x20) ? colors[2] : colors[1], 1);
	draw_string(x +  45, y, "I", ((gb.memory[0xFF40] & 0x10) == 0x10) ? colors[2] : colors[1], 1);
	draw_string(x +  60, y, "T", ((gb.memory[0xFF40] & 0x08) == 0x08) ? colors[2] : colors[1], 1);
	draw_string(x +  75, y, "8", ((gb.memory[0xFF40] & 0x04) == 0x04) ? colors[2] : colors[1], 1);
	draw_string(x +  90, y, "S", ((gb.memory[0xFF40] & 0x02) == 0x02) ? colors[2] : colors[1], 1);
	draw_string(x + 105, y, "B", ((gb.memory[0xFF40] & 0x01) == 0x01) ? colors[2] : colors[1], 1);

	draw_string(x     ,  y+15, "L", ((gb.memory[0xFF41] & 0x40) == 0x40) ? colors[2] : colors[1], 1);
	draw_string(x + 15,  y+15, "O", ((gb.memory[0xFF41] & 0x20) == 0x20) ? colors[2] : colors[1], 1);
	draw_string(x + 30,  y+15, "V", ((gb.memory[0xFF41] & 0x10) == 0x10) ? colors[2] : colors[1], 1);
	draw_string(x + 45,  y+15, "H", ((gb.memory[0xFF41] & 0x08) == 0x08) ? colors[2] : colors[1], 1);
	draw_string(x + 60,  y+15, "C", ((gb.memory[0xFF41] & 0x04) == 0x04) ? colors[2] : colors[1], 1);
	draw_string(x + 75,  y+15, "M", ((gb.memory[0xFF41] & 0x02) == 0x02) ? colors[2] : colors[1], 1);
	draw_string(x + 90,  y+15, "M", ((gb.memory[0xFF41] & 0x01) == 0x01) ? colors[2] : colors[1], 1);

	draw_string(x, y + 30, "LY:  " + hex(gb.memory[0xFF44], 2), colors[3], 1);
	draw_string(x, y + 45, "LYC: " + hex(gb.memory[0xFF45], 2), colors[3], 1);
	draw_string(x, y + 60, "WX:  " + hex(gb.memory[0xFF4B], 2), colors[3], 1);
	draw_string(x, y + 75, "WY:  " + hex(gb.memory[0xFF4A], 2), colors[3], 1);
	draw_string(x, y + 90, "ILY: " + hex(gb.ppu.internal_scanline, 2), colors[3], 1);
}

static inline void draw_int(int x, int y)
{
	draw_string(x     , y + 10, "V", ((gb.memory[0xFF0F] & 0x01) == 0x01) ? colors[2] : colors[1], 1);
	draw_string(x + 15, y + 10, "L", ((gb.memory[0xFF0F] & 0x02) == 0x02) ? colors[2] : colors[1], 1);
	draw_string(x + 30, y + 10, "T", ((gb.memory[0xFF0F] & 0x04) == 0x04) ? colors[2] : colors[1], 1);
	draw_string(x + 45, y + 10, "S", ((gb.memory[0xFF0F] & 0x08) == 0x08) ? colors[2] : colors[1], 1);
	draw_string(x + 60, y + 10, "J", ((gb.memory[0xFF0F] & 0x10) == 0x10) ? colors[2] : colors[1], 1);
	draw_string(x, y - 10, "I", (gb.cpu.iff) ? colors[2] : colors[1], 1);

	draw_string(x,      y, "V", ((gb.memory[0xFFFF] & 0x01) == 0x01) ? colors[2] : colors[1], 1);
	draw_string(x + 15, y, "L", ((gb.memory[0xFFFF] & 0x02) == 0x02) ? colors[2] : colors[1], 1);
	draw_string(x + 30, y, "T", ((gb.memory[0xFFFF] & 0x04) == 0x04) ? colors[2] : colors[1], 1);
	draw_string(x + 45, y, "S", ((gb.memory[0xFFFF] & 0x08) == 0x08) ? colors[2] : colors[1], 1);
	draw_string(x + 60, y, "J", ((gb.memory[0xFFFF] & 0x10) == 0x10) ? colors[2] : colors[1], 1);
}

static inline void createFont()
{
	std::string data;
	data += "?Q`0001oOch0o01o@F40o0<AGD4090LAGD<090@A7ch0?00O7Q`0600>00000000";
	data += "O000000nOT0063Qo4d8>?7a14Gno94AA4gno94AaOT0>o3`oO400o7QN00000400";
	data += "Of80001oOg<7O7moBGT7O7lABET024@aBEd714AiOdl717a_=TH013Q>00000000";
	data += "720D000V?V5oB3Q_HdUoE7a9@DdDE4A9@DmoE4A;Hg]oM4Aj8S4D84@`00000000";
	data += "OaPT1000Oa`^13P1@AI[?g`1@A=[OdAoHgljA4Ao?WlBA7l1710007l100000000";
	data += "ObM6000oOfMV?3QoBDD`O7a0BDDH@5A0BDD<@5A0BGeVO5ao@CQR?5Po00000000";
	data += "Oc``000?Ogij70PO2D]??0Ph2DUM@7i`2DTg@7lh2GUj?0TO0C1870T?00000000";
	data += "70<4001o?P<7?1QoHg43O;`h@GT0@:@LB@d0>:@hN@L0@?aoN@<0O7ao0000?000";
	data += "OcH0001SOglLA7mg24TnK7ln24US>0PL24U140PnOgl0>7QgOcH0K71S0000A000";
	data += "00H00000@Dm1S007@DUSg00?OdTnH7YhOfTL<7Yh@Cl0700?@Ah0300700000000";
	data += "<008001QL00ZA41a@6HnI<1i@FHLM81M@@0LG81?O`0nC?Y7?`0ZA7Y300080000";
	data += "O`082000Oh0827mo6>Hn?Wmo?6HnMb11MP08@C11H`08@FP0@@0004@000000000";
	data += "00P00001Oab00003OcKP0006@6=PMgl<@440MglH@000000`@000001P00000000";
	data += "Ob@8@@00Ob@8@Ga13R@8Mga172@8?PAo3R@827QoOb@820@0O`0007`0000007P0";
	data += "O`000P08Od400g`<3V=P0G`673IP0`@3>1`00P@6O`P00g`<O`000GP800000000";
	data += "?P9PL020O`<`N3R0@E4HC7b0@ET<ATB0@@l6C4B0O`H3N7b0?P01L3R000000020";

	int px = 0, py = 0;
	for (int b = 0; b < 1024; b += 4)
	{
		uint32_t sym1 = (uint32_t)data[b + 0] - 48;
		uint32_t sym2 = (uint32_t)data[b + 1] - 48;
		uint32_t sym3 = (uint32_t)data[b + 2] - 48;
		uint32_t sym4 = (uint32_t)data[b + 3] - 48;
		uint32_t r = sym1 << 18 | sym2 << 12 | sym3 << 6 | sym4;

		for (int i = 0; i < 24; i++)
		{
			int k = r & (1 << i) ? 255 : 0;
			font[px + py * 128] = k | (k << 8) | (k << 16) | (k << 24);
			if (++py == 48) { px++; py = 0; }
		}
	}
}

static inline void disassemble(word start, word end)
{
	disassembly.clear();

	uint32_t addr = start;
	word line_addr = 0;

	while (addr <= end)
	{
		line_addr = addr;

		std::string instruction = "$" + hex(addr, 4) + ": ";

		byte opcode = gb.memory[addr]; addr++;
		
		switch (opcode)
		{
		case 0x00: instruction += "NOP"; break;
		case 0x10: instruction += "STOP"; addr++; break;	// STOP
		case 0x76: instruction += "HALT"; break;	// HALT
		case 0xF3: instruction += "DI"; break;		// DI
		case 0xFB: instruction += "EI";; break;		// EI

		case 0x02: instruction += "LD (BC), A"; break;	// 
		case 0x12: instruction += "LD (DE), A"; break;	// 
		case 0x22: instruction += "LD (HL+), A"; break;
		case 0x32: instruction += "LD (HL-), A"; break;
		case 0x0A: instruction += "LD A, (BC)"; break;	// LD A, (BC)
		case 0x1A: instruction += "LD A, (DE)"; break;
		case 0x2A: instruction += "LD A, (HL+)"; break;	// LD A, (HL+)
		case 0x3A: instruction += "LD A, (HL-)"; break;	// LD A, (HL-)

		case 0x06: instruction += "LD B, " + hex(gb.memory[addr], 2); addr++; break;
		case 0x16: instruction += "LD D, " + hex(gb.memory[addr], 2); addr++; break;
		case 0x26: instruction += "LD H, " + hex(gb.memory[addr], 2); addr++; break;
		case 0x36: instruction += "LD (HL), " + hex(gb.memory[addr], 2); addr++; break;
		case 0x0E: instruction += "LD C, " + hex(gb.memory[addr], 2); addr++; break;
		case 0x1E: instruction += "LD E, " + hex(gb.memory[addr], 2); addr++; break;
		case 0x2E: instruction += "LD L, " + hex(gb.memory[addr], 2); addr++; break;
		case 0x3E: instruction += "LD A, " + hex(gb.memory[addr], 2); addr++; break;

		case 0x40: instruction += "LD B, B"; break;	// LD B, B
		case 0x41: instruction += "LD B, C"; break;	// LD B, C
		case 0x42: instruction += "LD B, D"; break;
		case 0x43: instruction += "LD B, E"; break;	// LD B, E
		case 0x44: instruction += "LD B, H"; break;	// LD B, H
		case 0x45: instruction += "LD B, L"; break;	// LD B, L
		case 0x46: instruction += "LD B, (HL)"; break;	// LD B, (HL)
		case 0x47: instruction += "LD B, A"; break;	// LD B, A
		case 0x48: instruction += "LD C, B"; break;	// LD C, B
		case 0x49: instruction += "LD C, C"; break;	// LD C, C
		case 0x4A: instruction += "LD C, D"; break;	// LD C, D
		case 0x4B: instruction += "LD C, E"; break;	// LD C, E
		case 0x4C: instruction += "LD C, H"; break;	// LD C, H
		case 0x4D: instruction += "LD C, L"; break;	// LD C, L
		case 0x4E: instruction += "LD C, (HL)"; break;	// LD C, (HL)
		case 0x4F: instruction += "LD C, A"; break;

		case 0x50: instruction += "LD D, B"; break;	// LD D, B
		case 0x51: instruction += "LD D, C"; break;	// LD D, C
		case 0x52: instruction += "LD D, D"; break;	// LD D, D
		case 0x53: instruction += "LD D, E"; break;	// LD D, E
		case 0x54: instruction += "LD D, H"; break;	// LD D, H
		case 0x55: instruction += "LD D, L"; break;	// LD D, L
		case 0x56: instruction += "LD D, (HL)"; break;	// LD D, (HL)
		case 0x57: instruction += "LD D, A"; break;
		case 0x58: instruction += "LD E, B"; break;	// LD E, B
		case 0x59: instruction += "LD E, C"; break;	// LD E, C
		case 0x5A: instruction += "LD E, D"; break;	// LD E, D
		case 0x5B: instruction += "LD E, E"; break;	// LD E, E
		case 0x5C: instruction += "LD E, H"; break;	// LD E, H
		case 0x5D: instruction += "LD E, L"; break;	// LD E, L
		case 0x5E: instruction += "LD E, (HL)"; break;	// LD E, (HL)
		case 0x5F: instruction += "LD E, A"; break;	// LD E, A

		case 0x60: instruction += "LD H, B"; break;	// LD H, B
		case 0x61: instruction += "LD H, C"; break;	// LD H, C
		case 0x62: instruction += "LD H, D"; break;	// LD H, D
		case 0x63: instruction += "LD H, E"; break;	// LD H, E
		case 0x64: instruction += "LD H, H"; break;	// LD H, H
		case 0x65: instruction += "LD H, L"; break;	// LD H, L
		case 0x66: instruction += "LD H, (HL)"; break;	// LD H, (HL)
		case 0x67: instruction += "LD H, A"; break;
		case 0x68: instruction += "LD L, B"; break;	// LD L, B
		case 0x69: instruction += "LD L, C"; break;	// LD L, C
		case 0x6A: instruction += "LD L, D"; break;	// LD L, D
		case 0x6B: instruction += "LD L, E"; break;	// LD L, E
		case 0x6C: instruction += "LD L, H"; break;	// LD L, H
		case 0x6D: instruction += "LD L, L"; break;	// LD L, L
		case 0x6E: instruction += "LD L, (HL)"; break;	// LD L, (HL)
		case 0x6F: instruction += "LD L, A"; break;	// LD L, A

		case 0x70: instruction += "LD (HL), B"; break;	// LD (HL), B
		case 0x71: instruction += "LD (HL), C"; break;	// LD (HL), C
		case 0x72: instruction += "LD (HL), D"; break;	// LD (HL), D
		case 0x73: instruction += "LD (HL), E"; break;	// LD (HL), E
		case 0x74: instruction += "LD (HL), H"; break;	// LD (HL), H
		case 0x75: instruction += "LD (HL), L"; break;	// LD (HL), L
		case 0x77: instruction += "LD (HL), A"; break;
		case 0x78: instruction += "LD A, B"; break;
		case 0x79: instruction += "LD A, C"; break;	// LD A, C
		case 0x7A: instruction += "LD A, D"; break;	// LD A, D
		case 0x7B: instruction += "LD A, E"; break;
		case 0x7C: instruction += "LD A, H"; break;
		case 0x7D: instruction += "LD A, L"; break;	// LD A, L
		case 0x7E: instruction += "LD A, (HL)"; break;	// LD A, (HL)
		case 0x7F: instruction += "LD A, A"; break;	// LD A, A

		case 0xE0: instruction += "LDH (" + hex(gb.memory[addr], 2) + "), A"; addr++; break;
		case 0xF0: instruction += "LDH A, (" + hex(gb.memory[addr], 2) + ")"; addr++; break;
		case 0xE2: instruction += "LD (C), A"; break;
		case 0xF2: instruction += "LD A, (C)"; break;		// LD A, (C)
		case 0xEA: instruction += "LD (" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4) + "), A"; addr += 2; break;
		case 0xFA: instruction += "LD A, (" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4) + ")"; addr += 2; break;		// LD A, (a16)
		case 0x01: instruction += "LD BC, " + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2; break;				// LD BC, d16
		case 0x11: instruction += "LD DE, " + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2; break;
		case 0x21: instruction += "LD HL, " + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2; break;
		case 0x31: instruction += "LD SP, " + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2; break;

		case 0x08: instruction += "LD (" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4) + "), SP"; addr += 2; break;	// LD (a16), SP
		case 0xF8: instruction += "LD HL, SP + " + hex(gb.memory[addr], 2); addr++; break;					// LD HL, SP + r8
		case 0xF9: instruction += "LD SP, HL"; break;		// LD SP, HL

		case 0xC1: instruction += "POP BC"; break;
		case 0xD1: instruction += "POP DE"; break;	// POP DE
		case 0xE1: instruction += "POP HL"; break;	// POP HL
		case 0xF1: instruction += "POP AF"; break;			// POP AF

		case 0xC5: instruction += "PUSH BC"; break;
		case 0xD5: instruction += "PUSH DE"; break;	// PUSH DE
		case 0xE5: instruction += "PUSH HL"; break;	// PUSH HL
		case 0xF5: instruction += "PUSH AF"; break;			// PUSH AF

		case 0x04: instruction += "INC B"; break;
		case 0x14: instruction += "INC D"; break;	// INC D
		case 0x24: instruction += "INC H"; break;
		case 0x34: instruction += "INC (HL)"; break;	// INC (HL)
		case 0x0C: instruction += "INC C"; break;
		case 0x1C: instruction += "INC E"; break;	// INC E
		case 0x2C: instruction += "INC L"; break;	// INC L
		case 0x3C: instruction += "INC A"; break;

		case 0x05: instruction += "DEC B"; break;
		case 0x15: instruction += "DEC D"; break;
		case 0x25: instruction += "DEC H"; break;	// DEC H
		case 0x35: instruction += "DEC (HL)"; break;	// DEC (HL)
		case 0x0D: instruction += "DEC C"; break;
		case 0x1D: instruction += "DEC E"; break;
		case 0x2D: instruction += "DEC L"; break;	// DEC L
		case 0x3D: instruction += "DEC A"; break;

		case 0x27: instruction += "DAA"; break; // DAA
		case 0x37: instruction += "SCF"; break; // SCF
		case 0x2F: instruction += "CPL"; break; // CPL
		case 0x3F: instruction += "CCF"; break; // CCF

		case 0x80: instruction += "ADD B"; break;	// ADD B
		case 0x81: instruction += "ADD C"; break;	// ADD C
		case 0x82: instruction += "ADD D"; break;	// ADD D
		case 0x83: instruction += "ADD E"; break;	// ADD E
		case 0x84: instruction += "ADD H"; break;	// ADD H
		case 0x85: instruction += "ADD L"; break;	// ADD L
		case 0x86: instruction += "ADD (HL)"; break;
		case 0x87: instruction += "ADD A"; break;	// ADD A
		case 0x88: instruction += "ADC B"; break; // ADC B
		case 0x89: instruction += "ADC C"; break; // ADC C
		case 0x8A: instruction += "ADC D"; break; // ADC D
		case 0x8B: instruction += "ADC E"; break; // ADC E
		case 0x8C: instruction += "ADC H"; break; // ADC H
		case 0x8D: instruction += "ADC L"; break; // ADC L
		case 0x8E: instruction += "ADC (HL)"; break; // ADC (HL)
		case 0x8F: instruction += "ADC A"; break; // ADC A

		case 0x90: instruction += "SUB B"; break;
		case 0x91: instruction += "SUB C"; break;	// SUB C
		case 0x92: instruction += "SUB D"; break;	// SUB D
		case 0x93: instruction += "SUB E"; break;	// SUB E
		case 0x94: instruction += "SUB H"; break;	// SUB H
		case 0x95: instruction += "SUB L"; break;	// SUB L
		case 0x96: instruction += "SUB (HL)"; break;	// SUB (HL)
		case 0x97: instruction += "SUB A"; break;	// SUB A
		case 0x98: instruction += "SBC B"; break; // SBC B
		case 0x99: instruction += "SBC C"; break; // SBC C
		case 0x9A: instruction += "SBC D"; break; // SBC D
		case 0x9B: instruction += "SBC E"; break; // SBC E
		case 0x9C: instruction += "SBC H"; break; // SBC H
		case 0x9D: instruction += "SBC L"; break; // SBC L
		case 0x9E: instruction += "SBC (HL)"; break;	// SBC (HL)
		case 0x9F: instruction += "SBC A"; break;	// SBC A

		case 0xA0: instruction += "AND B"; break;	// AND B
		case 0xA1: instruction += "AND C"; break;	// AND C
		case 0xA2: instruction += "AND D"; break;	// AND D
		case 0xA3: instruction += "AND E"; break;	// AND E
		case 0xA4: instruction += "AND H"; break;	// AND H
		case 0xA5: instruction += "AND L"; break;
		case 0xA6: instruction += "AND (HL)"; break;	// AND (HL)
		case 0xA7: instruction += "AND A"; break;	// AND A
		case 0xA8: instruction += "XOR B"; break;	// XOR B
		case 0xA9: instruction += "XOR C"; break;	// XOR C
		case 0xAA: instruction += "XOR D"; break;	// XOR D
		case 0xAB: instruction += "XOR E"; break;	// XOR E
		case 0xAC: instruction += "XOR H"; break;	// XOR H
		case 0xAD: instruction += "XOR L"; break;	// XOR L
		case 0xAE: instruction += "XOR (HL)"; break;	// XOR (HL)
		case 0xAF: instruction += "XOR A"; break;

		case 0xB0: instruction += "OR B"; break;	// OR B
		case 0xB1: instruction += "OR C"; break;	// OR C
		case 0xB2: instruction += "OR D"; break;	// OR D
		case 0xB3: instruction += "OR E"; break;	// OR E
		case 0xB4: instruction += "OR H"; break;	// OR H
		case 0xB5: instruction += "OR L"; break;	// OR L
		case 0xB6: instruction += "OR (HL)"; break;	// OR (HL)
		case 0xB7: instruction += "OR A"; break;	// OR A
		case 0xB8: instruction += "CP B"; break;	// CP B
		case 0xB9: instruction += "CP C"; break;
		case 0xBA: instruction += "CP D"; break;	// CP D
		case 0xBB: instruction += "CP E"; break;	// CP E
		case 0xBC: instruction += "CP H"; break;	// CP H
		case 0xBD: instruction += "CP L"; break;	// CP L
		case 0xBE: instruction += "CP (HL)"; break;
		case 0xBF: instruction += "CP A"; break;	// CP A

		case 0xC6: instruction += "ADD " + hex(gb.memory[addr], 2); addr++; break;	// ADD d8
		case 0xD6: instruction += "SUB " + hex(gb.memory[addr], 2); addr++; break;	// SUB d8
		case 0xE6: instruction += "AND " + hex(gb.memory[addr], 2); addr++; break;	// AND d8
		case 0xF6: instruction += "OR " + hex(gb.memory[addr], 2); addr++; break;	// OR d8
		case 0xCE: instruction += "ADC " + hex(gb.memory[addr], 2); addr++; break;	// ADC d8
		case 0xDE: instruction += "SBC " + hex(gb.memory[addr], 2); addr++; break;	// SBC d8
		case 0xEE: instruction += "XOR " + hex(gb.memory[addr], 2); addr++; break;	// XOR d8
		case 0xFE: instruction += "CP " + hex(gb.memory[addr], 2); addr++; break;

		case 0x03: instruction += "INC BC"; break;	// INC BC
		case 0x13: instruction += "INC DE"; break;
		case 0x23: instruction += "INC HL"; break;
		case 0x33: instruction += "INC SP"; break;
		case 0x0B: instruction += "DEC BC"; break;
		case 0x1B: instruction += "DEC DE"; break;
		case 0x2B: instruction += "DEC HL"; break;
		case 0x3B: instruction += "DEC SP"; break;

		case 0x09: instruction += "ADD HL, BC"; break;	// ADD HL, BC
		case 0x19: instruction += "ADD HL, DE"; break;
		case 0x29: instruction += "ADD HL, HL"; break;	// ADD HL, HL
		case 0x39: instruction += "ADD HL, SP"; break;		// ADD HL, SP
		case 0xE8: instruction += "ADD SP, " + hex(gb.memory[addr], 2); addr++; break;				// ADD SP, r8

		case 0x18: instruction += "JR " + hex(gb.memory[addr], 2); addr++; break;
		case 0x20: instruction += "JR NZ, " + hex(gb.memory[addr], 2); addr++; break;
		case 0x28: instruction += "JR Z, " + hex(gb.memory[addr], 2); addr++; break;
		case 0x30: instruction += "JR NC, " + hex(gb.memory[addr], 2); addr++; break;	// JR NC, r8
		case 0x38: instruction += "JR C, " + hex(gb.memory[addr], 2); addr++; break;	// JR C, r8

		case 0xC3: instruction += "JP, $" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2;  break;	// JP, a16
		case 0xE9: instruction += "JP HL"; break;	// JP, a16
		case 0xC2: instruction += "JP NZ, $" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2;  break;	// JP NZ, a16
		case 0xCA: instruction += "JP Z, $" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2;  break;	// JP Z, a16
		case 0xD2: instruction += "JP NC, $" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2;  break;	// JP NC, a16
		case 0xDA: instruction += "JP C, $" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2;  break;	// JP C, a16

		case 0xCD: instruction += "CALL $" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2; break;
		case 0xC4: instruction += "CALL NZ, $" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2; break; // CALL NZ, a16
		case 0xCC: instruction += "CALL Z, $" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2; break; // CALL Z, a16
		case 0xD4: instruction += "CALL NC, $" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2; break; // CALL NC, a16
		case 0xDC: instruction += "CALL C, $" + hex(gb.memory[addr] | (gb.memory[addr + 1] << 8), 4); addr += 2; break; // CALL C, a16

		case 0xC9: instruction += "RET"; break;
		case 0xD9: instruction += "RETI"; break;	// RETI
		case 0xC0: instruction += "RET NZ"; break;	// RET NZ
		case 0xC8: instruction += "RET Z"; break;	// RET Z
		case 0xD0: instruction += "RET NC"; break;	// RET NC
		case 0xD8: instruction += "RET C"; break;	// RET C

		case 0xC7: instruction += "RST 00"; break; // RST 00
		case 0xD7: instruction += "RST 10"; break; // RST 10
		case 0xE7: instruction += "RST 20"; break; // RST 20
		case 0xF7: instruction += "RST 30"; break; // RST 30
		case 0xCF: instruction += "RST 08"; break; // RST 08
		case 0xDF: instruction += "RST 18"; break; // RST 18
		case 0xEF: instruction += "RST 28"; break; // RST 28
		case 0xFF: instruction += "RST 38"; break; // RST 38

		case 0x07: instruction += "RLCA"; break;	// RLCA
		case 0x17: instruction += "RLA"; break;
		case 0x0F: instruction += "RRCA"; break;	// RRCA
		case 0x1F: instruction += "RRA"; break;	// RRA

		case 0xCB: instruction += "CB " + hex(gb.memory[addr], 2); addr += 1; break;

		default: instruction += hex(opcode, 2); break;
		}

		disassembly[line_addr] = instruction;
	}
}

#endif

static inline int start()
{
	gameboy_init(&gb);
	
#ifdef DEBUG
	clear(colors[0]);
	createFont();
	disassemble(0x0000, 0xFFFF);
#endif

	if (cartridge_load(&gb.cartridge, "roms/pokemongold.gbc", 0x0000) != 0) return 1;
			
	return 0;
}

static inline void render()
{
#ifdef DEBUG
	clear(colors[0]);
	draw_screen(6, 3, false, false);
	draw_lcd(16+32*8, 13 + 144);
	draw_tiles(0, 13 + 144);
	draw_tilemap(0, 256, true);
#endif

	int pitch = 0;
	void* pixels = NULL;

	if (SDL_LockTexture(texture, NULL, &pixels, &pitch) != 0)
	{
		SDL_Log("unable to lock texture: %s", SDL_GetError());
	}
	else
	{
#ifdef DEBUG
		memcpy(pixels, screen, pitch * SCREEN_HEIGHT);
#else
		memcpy(pixels, gb.ppu.screen_buffer, pitch * SCREEN_HEIGHT);
#endif
	}

	SDL_UnlockTexture(texture);

	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

static inline void update()
{
	current_time = SDL_GetTicks();
	dt = current_time - last_time;

	word bp = 0x1ff2;
	bool be = false;

	while (SDL_PollEvent(&e))
	{
		if (e.type == SDL_QUIT)
		{
			should_quit = true;
		}

		else if (e.type == SDL_KEYDOWN)
		{
			SDL_Scancode key = e.key.keysym.scancode;
			switch (key)
			{
#ifdef DEBUG
			case SDL_SCANCODE_SPACE:
				if (paused)
				{
					int cyc = gb.cpu.cyc;
					g80_step(&gb.cpu);
					int elapsed = gb.cpu.cyc - cyc;
					ppu_step(&gb.ppu, elapsed);
				}
				break;

			case SDL_SCANCODE_F:
				if (paused)
				{
					int count = 0;
					while (count < dt * CLOCKSPEED / 1000)
					{
						int cyc = gb.cpu.cyc;
						g80_step(&gb.cpu);
						int elapsed = gb.cpu.cyc - cyc;
						count += elapsed;

						timers_step(&gb.timers, elapsed);

						ppu_step(&gb.ppu, elapsed);

						process_interrupts(&gb.cpu);

						if (be && gb.cpu.pc == bp)
						{
							gb.cpu.bp = true;
							paused = true;
							break;
						}
					}
				}
				break;

			case SDL_SCANCODE_D: disassemble(0x0000, 0xFFFF); break;
#endif
			case SDL_SCANCODE_P: paused = !paused; break;
			case SDL_SCANCODE_A: gb.joypad &= ~0x8; break;
			case SDL_SCANCODE_S: gb.joypad &= ~0x4; break;
			case SDL_SCANCODE_Z: gb.joypad &= ~0x2; break;
			case SDL_SCANCODE_X: gb.joypad &= ~0x1; break;

			case SDL_SCANCODE_DOWN:  gb.joypad &= ~0x80; break;
			case SDL_SCANCODE_UP:    gb.joypad &= ~0x40; break;
			case SDL_SCANCODE_LEFT:  gb.joypad &= ~0x20; break;
			case SDL_SCANCODE_RIGHT: gb.joypad &= ~0x10; break;

			}
		}
		else if (e.type == SDL_KEYUP)
		{
			SDL_Scancode key = e.key.keysym.scancode;
			switch (key)
			{
			case SDL_SCANCODE_A: gb.joypad |= 0x8; break;
			case SDL_SCANCODE_S: gb.joypad |= 0x4; break;
			case SDL_SCANCODE_Z: gb.joypad |= 0x2; break;
			case SDL_SCANCODE_X: gb.joypad |= 0x1; break;

			case SDL_SCANCODE_DOWN:  gb.joypad |= 0x80; break;
			case SDL_SCANCODE_UP:    gb.joypad |= 0x40; break;
			case SDL_SCANCODE_LEFT:  gb.joypad |= 0x20; break;
			case SDL_SCANCODE_RIGHT: gb.joypad |= 0x10; break;
			}
		}
	}

	if (!paused)
	{
		int count = 0;
		while (count < dt * CLOCKSPEED / 1000)
		{
			int cyc = gb.cpu.cyc;
			g80_step(&gb.cpu);
			int elapsed = gb.cpu.cyc - cyc;
			count += elapsed;

			timers_step(&gb.timers, elapsed);

			ppu_step(&gb.ppu, elapsed);

			process_interrupts(&gb.cpu);

			if (be && gb.cpu.pc == bp)
			{
				gb.cpu.bp = true;
				paused = true;
				break;
			}
		}
	}

	render();

	last_time = current_time;
}

int main(int argc, char** argv)
{
	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window* window = SDL_CreateWindow("Gameboy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH*SCALE, SCREEN_HEIGHT*SCALE, SDL_WINDOW_SHOWN);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

	if (start() != 0)
		return 1;

	render();

	while (!should_quit)
	{
		update();
	}

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}