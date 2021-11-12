#define _CRT_SECURE_NO_WARNINGS

#include "emulator.h"

static byte rb(void* userdata, word addr)
{
	gameboy* gb = (gameboy*)userdata;
	
	if (addr < 0x100 && gb->memory[0xFF50] == 0)
	{
		return gb->bootrom[addr];
	}

	if (addr < 0x4000)
	{
		return gb->cartridge[addr];
	}

	if (addr >= 0x4000 && addr < 0x8000)
	{
		return gb->cartridge[addr - 0x4000 + gb->rom_bank * 0x4000];
	}

	if (addr >= 0xA000 && addr < 0xC000)
	{
		if (gb->mbc == 3 && gb->rtc_enable)
		{
			return 0x00;
		}

		if (gb->ram_enable)
		{
			return gb->ram[addr - 0xA000 + gb->ram_bank * 0x2000];
		}

		return 0xFF;
	}

	if (addr == 0xFF00)
	{
		if ((gb->memory[0xFF00] & 0x20) == 0)
			return gb->joypad & 0xF;
		else if ((gb->memory[0xFF00] & 0x10) == 0)
			return gb->joypad >> 4;
		return 0xFF;
	}

	if (addr == 0xFF04)
	{
		return gb->cpu.div;
	}

	return gb->memory[addr];
}

static void wb(void* userdata, word addr, byte val)
{	
	gameboy* gb = (gameboy*)userdata;
	
	if (addr < 0x100 && gb->memory[0xFF50] == 0)
	{
		printf("cant write to boot rom!\n");
	}

	else if (addr < 0x8000)
	{
		// printf("writing to rom addr: %04x (%02X)!\n", addr, val);

		if (addr < 0x2000 && (gb->mbc == 1 || gb->mbc == 2 || gb->mbc == 3))
		{
			if (gb->mbc == 2 && (addr & 0x10) == 0x10)
				return;

			if (gb->mbc == 3)
				gb->rtc_enable = (val & 0xF) == 0xA;

			gb->ram_enable = (val & 0xF) == 0xA;
			
		}
		else if (addr >= 0x2000 && addr < 0x4000 && (gb->mbc == 1 || gb->mbc == 2 || gb->mbc == 3))
		{
			if (gb->mbc == 1)
			{
				gb->rom_bank &= ~0x1F;
				gb->rom_bank |= (val & 0x1F);
				
			}
			else if (gb->mbc == 2)
			{
				gb->rom_bank = val & 0xF;
			}
			else if (gb->mbc == 3)
			{
				gb->rom_bank &= ~0x7F;
				gb->rom_bank |= (val & 0x7F);
			}

			if (gb->rom_bank == 0)
				gb->rom_bank = 1;
		}
		else if (addr >= 0x4000 && addr < 0x6000)
		{
			if (gb->mbc == 1)
			{
				if (gb->rom_enable)
				{
					gb->rom_bank &= ~0xE0;
					gb->rom_bank |= (val & 0xE0);
					if (gb->rom_bank == 0)
						gb->rom_bank = 1;
				}
				else
				{
					gb->ram_bank = val & 0x3;
				}
			}
			else if (gb->mbc == 3)
			{
				if (val <= 0x3)
				{
					gb->ram_bank = val & 0x3;
					gb->rtc_enable = false;
				}
				else if (val >= 0x8 && val <= 0xC)
				{
					gb->rtc_enable = true;
				}
			}
		}
		else if (addr >= 0x6000)
		{
			if (gb->mbc == 1)
			{
				gb->rom_enable = (val & 0x1) == 0;
				if (gb->rom_enable)
					gb->ram_bank = 0;
			}
			else if (gb->mbc == 3)
			{
				if (gb->latch_zero && val == 1)
				{
					gb->latched = !gb->latched;
				}
				gb->latch_zero = (val == 0);
			}
		}
	}

	else if (addr >= 0xA000 && addr < 0xC000)
	{
		if (gb->rtc_enable)
		{
			// write to RTC
		}
		else if (gb->ram_enable)
		{
			gb->ram[addr - 0xA000 + gb->ram_bank * 0x2000] = val;
		}
	}

	else if (addr == 0xFF00)
	{
		gb->memory[addr] = val & 0xF0;
	}

	else if (addr == 0xFF04) // DIV
	{
		gb->cpu.div = 0;
	}

	else if (addr == 0xFF46) // DMA
	{
		addr = val << 8;
		for (int i = 0; i < 0xA0; i++)
		{
			gb->memory[0xFE00 + i] = gb->memory[addr + i];	// use memcpy?
		}
	}
	else
	{
		gb->memory[addr] = val;
	}
}

void gameboy_init(gameboy* const gb)
{
	g80_init(&gb->cpu);
	gb->cpu.userdata = gb;
	gb->cpu.read_byte = rb;
	gb->cpu.write_byte = wb;

	memset(gb->cartridge, 0, sizeof(gb->cartridge));
	memset(gb->memory, 0, sizeof(gb->memory));
	memset(gb->ram, 0, sizeof(gb->ram));

	memset(gb->cpu.screen_buffer, 0, sizeof(gb->cpu.screen_buffer));
	memset(gb->cpu.bg_buffer, 0, sizeof(gb->cpu.bg_buffer));
	memset(gb->cpu.wn_buffer, 0, sizeof(gb->cpu.wn_buffer));

	gb->memory[0xFF0F] = 0xe0;
	gb->memory[0xFF50] = 1;

	gb->joypad = 0xFF;
	gb->rom_bank = 1;
	gb->ram_bank = 0;
	gb->rom_enable = true;
	gb->ram_enable = false;
	gb->rtc_enable = false;
}

int gameboy_load_boot_rom(gameboy* const gb, const char* filename)
{
	FILE* f = fopen(filename, "rb");
	if (f == NULL)
	{
		return 1;
	}

	fseek(f, 0, SEEK_END);
	size_t file_size = ftell(f);
	rewind(f);

	if (file_size > 0x100)
	{
		return 1;
	}

	fread(&gb->bootrom[0], 1, file_size, f);

	fclose(f);
	return 0;
}

int gameboy_load_rom(gameboy* const gb, const char* filename, word addr)
{
	FILE* f = fopen(filename, "rb");
	if (f == NULL)
	{
		printf("cant open file!\n");
		return 1;
	}

	fseek(f, 0, SEEK_END);
	size_t file_size = ftell(f);
	rewind(f);

	if (file_size > sizeof(gb->cartridge))
	{
		printf("file too big!\n");
		return 1;
	}

	fread(&gb->cartridge, 1, file_size, f);

	fclose(f);
	return 0;
}