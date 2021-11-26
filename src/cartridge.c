#define _CRT_SECURE_NO_WARNINGS
#include "cartridge.h"

int cartidge_init(cartridge_t* const cart)
{
	memset(cart->rom, 0, sizeof(cart->rom));
	memset(cart->ram, 0, sizeof(cart->ram));

	cart->rom_bank = 1;
	cart->ram_bank = 0;
	cart->rom_enable = true;
	cart->ram_enable = false;
	cart->rtc_enable = false;

	cart->mbc = 0;
	cart->latch_zero = false;
	cart->latched = false;
}

int cartridge_load(cartridge_t* const cart, const char* filename, word addr)
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

	if (file_size > sizeof(cart->rom))
	{
		printf("file too big!\n");
		return 1;
	}

	fread(cart->rom, 1, file_size, f);

	fclose(f);

	switch (cart->rom[0x147])
	{
	case 0x0: cart->mbc = 0; break;
	case 0x1:
	case 0x2:
	case 0x3: cart->mbc = 1; break;
	case 0x5:
	case 0x6: cart->mbc = 2; break;
	case 0xF:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13: cart->mbc = 3; break;
	default: printf("MBC %02X not supported!\n", cart->rom[0x147]); return 1;
	}

	return 0;
}

bool cartridge_write(cartridge_t* const cart, word addr, byte val)
{
	if (addr < 0x2000 && (cart->mbc == 1 || cart->mbc == 2 || cart->mbc == 3))
	{
		if (cart->mbc == 2 && (addr & 0x10) == 0x10)
			return;

		if (cart->mbc == 3)
			cart->rtc_enable = (val & 0xF) == 0xA;

		cart->ram_enable = (val & 0xF) == 0xA;

	}
	else if (addr >= 0x2000 && addr < 0x4000 && (cart->mbc == 1 || cart->mbc == 2 || cart->mbc == 3))
	{
		if (cart->mbc == 1)
		{
			cart->rom_bank &= ~0x1F;
			cart->rom_bank |= (val & 0x1F);

		}
		else if (cart->mbc == 2)
		{
			cart->rom_bank = val & 0xF;
		}
		else if (cart->mbc == 3)
		{
			cart->rom_bank &= ~0x7F;
			cart->rom_bank |= (val & 0x7F);
		}

		if (cart->rom_bank == 0)
			cart->rom_bank = 1;
	}
	else if (addr >= 0x4000 && addr < 0x6000)
	{
		if (cart->mbc == 1)
		{
			if (cart->rom_enable)
			{
				cart->rom_bank &= ~0xE0;
				cart->rom_bank |= (val & 0xE0);
				if (cart->rom_bank == 0)
					cart->rom_bank = 1;
			}
			else
			{
				cart->ram_bank = val & 0x3;
			}
		}
		else if (cart->mbc == 3)
		{
			if (val <= 0x3)
			{
				cart->ram_bank = val & 0x3;
				cart->rtc_enable = false;
			}
			else if (val >= 0x8 && val <= 0xC)
			{
				cart->rtc_enable = true;
			}
		}
	}
	else if (addr >= 0x6000 && addr < 0x8000)
	{
		if (cart->mbc == 1)
		{
			cart->rom_enable = (val & 0x1) == 0;
			if (cart->rom_enable)
				cart->ram_bank = 0;
		}
		else if (cart->mbc == 3)
		{
			if (cart->latch_zero && val == 1)
			{
				cart->latched = !cart->latched;
			}
			cart->latch_zero = (val == 0);
		}
	}
	else if (addr >= 0xA000 && addr < 0xC000)
	{
		if (cart->rtc_enable)
		{
			// write to RTC
		}
		else if (cart->ram_enable)
		{
			cart->ram[addr - 0xA000 + cart->ram_bank * 0x2000] = val;
		}

		return true;
	}

	if (addr < 0x8000) return true;
	
	return false;
}

bool cartridge_read(cartridge_t* const cart, word addr, byte * val)
{
	if (addr < 0x4000)
	{
		*val = cart->rom[addr];
		return true;
	}

	if (addr < 0x8000)
	{
		*val = cart->rom[addr - 0x4000 + cart->rom_bank * 0x4000];
		return true;
	}

	if (addr >= 0xA000 && addr < 0xC000)
	{
		if (cart->mbc == 3 && cart->rtc_enable)
		{
			*val = 0x00;
			return true;
		}

		if (cart->ram_enable)
		{
			*val = cart->ram[addr - 0xA000 + cart->ram_bank * 0x2000];
			return true;
		}
	}

	return false;
}