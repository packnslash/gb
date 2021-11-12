#include "g80.h"

static const uint8_t OPCODES_CYCLES[256] = {
	 4, 12,  8,  8,  4,  4,  8,  4, 20,  8,  8,  8,  4,  4, 8,  4,
	 4, 12,  8,  8,  4,  4,  8,  4, 12,  8,  8,  8,  4,  4, 8,  4,
	 8, 12,  8,  8,  4,  4,  8,  4,  8,  8,  8,  8,  4,  4, 8,  4,
	 8, 12,  8,  8, 12, 12, 12,  4,  8,  8,  8,  8,  4,  4, 8,  4,
	 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4, 8,  4,
	 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4, 8,  4,
	 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4, 8,  4,
	 8,  8,  8,  8,  8,  8,  0,  8,  4,  4,  4,  4,  4,  4, 8,  4,
	 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4, 8,  4,
	 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4, 8,  4,
	 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4, 8,  4,
	 4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4, 8,  4,
	 8, 12, 12, 16, 12, 16,  8, 16,  8, 16, 12,  4, 12, 24, 8, 16,
	 8, 12, 12,  0, 12, 16,  8, 16,  8, 16, 12,  0, 12,  0, 8, 16,
	12, 12,  8,  0,  0, 16,  8, 16, 16,  4, 16,  0,  0,  0, 8, 16,
	12, 12,  8,  4,  0, 16,  8, 16, 12,  8, 16,  4,  0,  0, 8, 16
};

static const uint8_t CB_OPCODES_CYCLES[256] = {
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	 8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
	 8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
	 8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
	 8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	 8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
};

static inline byte rb(g80* const c, word addr)
{
	return c->read_byte(c->userdata, addr);
}

static inline word rw(g80* const c, word addr)
{
	return c->read_byte(c->userdata, addr) | c->read_byte(c->userdata, addr + 1) << 8;
}

static inline void wb(g80* const c, word addr, byte val)
{
	c->write_byte(c->userdata, addr, val);
}

static inline void ww(g80* const c, word addr, word val)
{
	c->write_byte(c->userdata, addr, val & 0xFF);
	c->write_byte(c->userdata, addr + 1, val >> 8);
}

static inline byte nextb(g80* const c)
{
	return rb(c, c->pc++);
}

static inline word nextw(g80* const c)
{
	word result = rw(c, c->pc);
	c->pc += 2;
	return result;
}

static inline void push(g80* const c, word val)
{
	c->sp -= 2;
	ww(c, c->sp, val);
}

static inline word pop(g80* const c)
{
	word val = rw(c, c->sp);
	c->sp += 2;
	return val;
}

static inline void push_af(g80* const c)
{
	byte f = 0;
	f |= (c->zf << 7);
	f |= (c->nf << 6);
	f |= (c->hf << 5);
	f |= (c->cf << 4);

	push(c, (c->a << 8) | f);
}

static inline void pop_af(g80* const c)
{
	word af = pop(c);

	c->a = af >> 8;
	c->zf = (af & 0x80) == 0x80;
	c->nf = (af & 0x40) == 0x40;
	c->hf = (af & 0x20) == 0x20;
	c->cf = (af & 0x10) == 0x10;
}

static inline void set_bc(g80* const c, word val)
{
	c->b = val >> 8;
	c->c = val & 0xFF;
}

static inline void set_de(g80* const c, word val)
{
	c->d = val >> 8;
	c->e = val & 0xFF;
}

static inline void set_hl(g80* const c, word val)
{
	c->h = val >> 8;
	c->l = val & 0xFF;
}

static inline word get_bc(g80* const c)
{
	return c->c | (c->b << 8);
}

static inline word get_de(g80* const c)
{
	return c->e | (c->d << 8);
}

static inline word get_hl(g80* const c)
{
	return c->l | (c->h << 8);
}

static inline bool carry(byte n, word a, word b, bool cy)
{
	int32_t result = a + b + cy;
	int32_t carry = result ^ a ^ b;
	return carry & (1 << n);
}

static inline byte inc(g80* const c, byte val)
{
	byte result = val + 1;
	c->zf = result == 0;
	c->nf = 0;
	c->hf = carry(4, val, 1, 0);
	return result;
}

static inline byte dec(g80* const c, byte val)
{
	byte result = val - 1;
	c->zf = result == 0;
	c->nf = 1;
	c->hf = !carry(4, val, ~1, 1);
	return result;
}

static inline void rlca(g80* const c)
{
	c->zf = 0;
	c->nf = 0;
	c->hf = 0;
	c->cf = c->a >> 7;
	c->a = (c->a << 1) | c->cf;
}

static inline void rla(g80* const c)
{
	bool cy = c->cf;
	c->zf = 0;
	c->nf = 0;
	c->hf = 0;
	c->cf = c->a >> 7;
	c->a = (c->a << 1) | cy;
}

static inline void rrca(g80* const c)
{
	c->zf = 0;
	c->nf = 0;
	c->hf = 0;
	c->cf = c->a & 1;
	c->a = (c->a >> 1) | (c->cf << 7);
}

static inline void rra(g80* const c)
{
	bool cy = c->cf;
	c->zf = 0;
	c->nf = 0;
	c->hf = 0;
	c->cf = c->a & 1;
	c->a = (c->a >> 1) | (cy << 7);
}

static inline void addhl(g80* const c, word val)
{
	uint32_t result = get_hl(c) + val;
	c->nf = 0;
	c->hf = carry(12, get_hl(c), val, 0);
	c->cf = carry(16, get_hl(c), val, 0);
	set_hl(c, get_hl(c) + val);
}

static inline void hlsp(g80* const c)
{
	int8_t rel = nextb(c);

	c->zf = 0;
	c->nf = 0;
	c->hf = carry(4, c->sp, rel, 0);
	c->cf = carry(8, c->sp, rel, 0);

	set_hl(c, c->sp + rel);
}

static inline void ldsp(g80* const c)
{
	int8_t rel = nextb(c);
	
	c->zf = 0;
	c->nf = 0;
	c->hf = carry(4, c->sp, rel, 0);
	c->cf = carry(8, c->sp, rel, 0);
	c->sp += rel;
}

static inline byte add(g80* const c, byte val, bool cy)
{
	byte result = c->a + val + cy;
	c->zf = result == 0;
	c->nf = 0;
	c->hf = carry(4, c->a, val, cy);
	c->cf = carry(8, c->a, val, cy);
	return result;
}

static inline byte sub(g80* const c, byte val, bool cy)
{
	byte result = add(c, ~val, !cy);
	c->nf = 1;
	c->hf = !c->hf;
	c->cf = !c->cf;
	return result;
}

static inline void and (g80* const c, byte val)
{
	c->a &= val;
	c->zf = c->a == 0;
	c->nf = 0;
	c->hf = 1;
	c->cf = 0;
}

static inline void xor(g80* const c, byte val)
{
	c->a ^= val;
	c->zf = c->a == 0;
	c->nf = 0;
	c->hf = 0;
	c->cf = 0;
}

static inline void or (g80* const c, byte val)
{
	c->a |= val;
	c->zf = c->a == 0;
	c->nf = 0;
	c->hf = 0;
	c->cf = 0;
}

static inline void cmp(g80* const c, byte val)
{
	sub(c, val, 0);
}

static inline void daa(g80* const c)
{
	//byte correction = 0;
	//
	//byte lsb = c->a & 0x0F;
	//byte msb = c->a >> 4;
	//
	//if (c->hf || lsb > 9)
	//{
	//	correction += 0x06;
	//}
	//
	//if (c->cf || msb > 9 || (msb >= 9 && lsb > 9))
	//{
	//	correction += 0x60;
	//	c->cf = 1;
	//}
	//
	//if (c->nf)
	//{
	//	c->hf = c->hf && lsb < 0x06;
	//	c->a -= correction;
	//}
	//else
	//{
	//	c->hf = lsb > 0x09;
	//	c->a += correction;
	//}
	//
	//c->zf = c->a == 0;

	byte correction = 0;

	const bool substraction = c->nf;
	if (substraction) {
		if (c->hf) {
			correction += 0x06;
		}

		if (c->cf) {
			correction += 0x60;
			c->cf = 1;
		}
		//c->hf = c->hf && (c->a & 0x0F) < 0x06;
		c->a -= correction;
	}
	else {
		if ((c->a & 0x0F) > 0x09 || c->hf) {
			correction += 0x06;
		}

		if (c->a > 0x99 || c->cf) {
			correction += 0x60;
			c->cf = 1;
		}

		//c->hf = (c->a & 0x0F) > 0x09;
		c->a += correction;
	}

	c->hf = 0;
	c->zf = c->a == 0;
}

static inline void jp(g80* const c, word addr)
{
	c->pc = addr;
}

static inline void jp_cond(g80* const c, bool condition)
{
	word addr = nextw(c);
	if (condition)
	{
		jp(c, addr);
		c->cyc += 4;
	}
}

static inline void jr(g80* const c, int8_t rel)
{
	c->pc += rel;
}

static inline void jr_cond(g80* const c, bool condition)
{
	int8_t rel = nextb(c);
	if (condition)
	{
		c->pc += rel;
		c->cyc += 4;
	}
}

static inline void call(g80* const c, word addr)
{
	push(c, c->pc);
	c->pc = addr;
}

static inline void call_cond(g80* const c, bool condition)
{
	word addr = nextw(c);
	if (condition)
	{
		call(c, addr);
		c->cyc += 12;
	}
}

static inline void ret(g80* const c)
{
	c->pc = pop(c);
}

static inline void ret_cond(g80* const c, bool condition)
{
	if (condition)
	{
		c->pc = pop(c);
		c->cyc += 12;
	}
}

static inline byte cb_rlc(g80* const c, byte val)
{
	bool cy = val >> 7;
	val = (val << 1) | cy;
	c->zf = val == 0;
	c->nf = 0;
	c->hf = 0;
	c->cf = cy;
	return val;
}

static inline byte cb_rrc(g80* const c, byte val)
{
	bool cy = val & 1;
	val = (val >> 1) | (cy << 7);
	c->zf = val == 0;
	c->nf = 0;
	c->hf = 0;
	c->cf = cy;
	return val;
}

static inline byte cb_rl(g80* const c, byte val)
{
	bool cy = c->cf;
	c->cf = val >> 7;
	val = (val << 1) | cy;
	c->zf = val == 0;
	c->nf = 0;
	c->hf = 0;
	return val;
}

static inline byte cb_rr(g80* const c, byte val)
{
	bool cy = c->cf;
	c->cf = val & 1;
	val = (val >> 1) | (cy << 7);
	c->zf = val == 0;
	c->nf = 0;
	c->hf = 0;
	return val;
}

static inline byte cb_sla(g80* const c, byte val)
{
	c->cf = val >> 7;
	val <<= 1;
	c->zf = val == 0;
	c->nf = 0;
	c->hf = 0;
	return val;
}

static inline byte cb_sra(g80* const c, byte val)
{
	c->cf = val & 1;
	val = (val >> 1) | (val & 0x80);
	c->zf = val == 0;
	c->nf = 0;
	c->hf = 0;
	return val;
}

static inline byte cb_swap(g80* const c, byte val)
{
	byte result = ((val >> 4) | (val << 4));
	c->zf = result == 0;
	c->nf = 0;
	c->hf = 0;
	c->cf = 0;
	return result;
}

static inline byte cb_srl(g80* const c, byte val)
{
	c->cf = val & 1;
	val >>= 1;
	c->zf = val == 0;
	c->nf = 0;
	c->hf = 0;
	return val;
}

static inline void cb_bit(g80* const c, byte val, byte n)
{
	byte result = val & (1 << n);

	c->zf = result == 0;
	c->nf = 0;
	c->hf = 1;
}

static inline void execute_opcode_cb(g80* const c, byte opcode)
{
	c->cyc += CB_OPCODES_CYCLES[opcode];

	byte x_ = (opcode >> 6) & 3;
	byte y_ = (opcode >> 3) & 7;
	byte z_ = opcode & 7;

	byte hl = 0;
	byte* reg = 0;

	switch (z_)
	{
	case 0: reg = &c->b; break;
	case 1: reg = &c->c; break;
	case 2: reg = &c->d; break;
	case 3: reg = &c->e; break;
	case 4: reg = &c->h; break;
	case 5: reg = &c->l; break;
	case 6:
		hl = rb(c, get_hl(c));
		reg = &hl;
		break;
	case 7: reg = &c->a; break;
	}

	switch (x_)
	{
	case 0: {
		switch (y_)
		{
		case 0: *reg = cb_rlc(c, *reg); break;
		case 1: *reg = cb_rrc(c, *reg); break;
		case 2: *reg = cb_rl(c, *reg); break;
		case 3: *reg = cb_rr(c, *reg); break;
		case 4: *reg = cb_sla(c, *reg); break;
		case 5: *reg = cb_sra(c, *reg); break;
		case 6: *reg = cb_swap(c, *reg); break;
		case 7: *reg = cb_srl(c, *reg); break;
		}
	} break;
	case 1: {
		cb_bit(c, *reg, y_);

	} break;
	case 2: *reg &= ~(1 << y_); break;
	case 3: *reg |= 1 << y_; break;
	}

	if (reg == &hl)
	{
		wb(c, get_hl(c), hl);
	}
}

static inline int execute_opcode(g80* const c, byte opcode)
{
	c->cyc += OPCODES_CYCLES[opcode];

	switch (opcode)
	{
	case 0x00: break;	// NOP
	case 0x10: nextb(c); break;	// STOP
	case 0x76: c->halted = 1; break;			// HALT
	case 0xF3: c->iff = 0; break;				// DI
	case 0xFB: c->interrupt_delay = 1; break;	// EI

	case 0x02: wb(c, get_bc(c), c->a); break;	// LD (BC), A
	case 0x12: wb(c, get_de(c), c->a); break;	// LD (DE), A 
	case 0x22: wb(c, get_hl(c), c->a); set_hl(c, get_hl(c) + 1); break;				// LD (HL+), A
	case 0x32: wb(c, get_hl(c), c->a); set_hl(c, get_hl(c) - 1); break;				// LD (HL-), A
	case 0x0A: c->a = rb(c, get_bc(c)); break;	// LD A, (BC)
	case 0x1A: c->a = rb(c, get_de(c)); break;			// LD A, (DE)
	case 0x2A: c->a = rb(c, get_hl(c)); set_hl(c, get_hl(c) + 1); break;	// LD A, (HL+)
	case 0x3A: c->a = rb(c, get_hl(c)); set_hl(c, get_hl(c) - 1); break;	// LD A, (HL-)

	case 0x06: c->b = nextb(c); break;					// LD B, d8
	case 0x16: c->d = nextb(c); break;					// LD D, d8
	case 0x26: c->h = nextb(c); break;					// LD H, d8
	case 0x36: wb(c, get_hl(c), nextb(c)); break;		// LD (HL), d8 
	case 0x0E: c->c = nextb(c); break;					// LD C, d8
	case 0x1E: c->e = nextb(c); break;					// LD E, d8
	case 0x2E: c->l = nextb(c); break;					// LD L, d8
	case 0x3E: c->a = nextb(c); break;					// LD A, d8

	case 0x40: c->b = c->b; break;	// LD B, B
	case 0x41: c->b = c->c; break;	// LD B, C
	case 0x42: c->b = c->d; break;	// LD B, D
	case 0x43: c->b = c->e; break;	// LD B, E
	case 0x44: c->b = c->h; break;	// LD B, H
	case 0x45: c->b = c->l; break;	// LD B, L
	case 0x46: c->b = rb(c, get_hl(c)); break;	// LD B, (HL)
	case 0x47: c->b = c->a; break;	// LD B, A
	case 0x48: c->c = c->b; break;	// LD C, B
	case 0x49: c->c = c->c; break;	// LD C, C
	case 0x4A: c->c = c->d; break;	// LD C, D
	case 0x4B: c->c = c->e; break;	// LD C, E
	case 0x4C: c->c = c->h; break;	// LD C, H
	case 0x4D: c->c = c->l; break;	// LD C, L
	case 0x4E: c->c = rb(c, get_hl(c)); break;	// LD C, (HL)
	case 0x4F: c->c = c->a; break;	// LD C, A

	case 0x50: c->d = c->b; break;	// LD D, B
	case 0x51: c->d = c->c; break;	// LD D, C
	case 0x52: c->d = c->d; break;	// LD D, D
	case 0x53: c->d = c->e; break;	// LD D, E
	case 0x54: c->d = c->h; break;	// LD D, H
	case 0x55: c->d = c->l; break;	// LD D, L
	case 0x56: c->d = rb(c, get_hl(c)); break;	// LD D, (HL)
	case 0x57: c->d = c->a; break;	// LD D, A
	case 0x58: c->e = c->b; break;	// LD E, B
	case 0x59: c->e = c->c; break;	// LD E, C
	case 0x5A: c->e = c->d; break;	// LD E, D
	case 0x5B: c->e = c->e; break;	// LD E, E
	case 0x5C: c->e = c->h; break;	// LD E, H
	case 0x5D: c->e = c->l; break;	// LD E, L
	case 0x5E: c->e = rb(c, get_hl(c)); break;	// LD E, (HL)
	case 0x5F: c->e = c->a; break;	// LD E, A

	case 0x60: c->h = c->b; break;	// LD H, B
	case 0x61: c->h = c->c; break;	// LD H, C
	case 0x62: c->h = c->d; break;	// LD H, D
	case 0x63: c->h = c->e; break;	// LD H, E
	case 0x64: c->h = c->h; break;	// LD H, H
	case 0x65: c->h = c->l; break;	// LD H, L
	case 0x66: c->h = rb(c, get_hl(c)); break;	// LD H, (HL)
	case 0x67: c->h = c->a; break;	// LD H, A
	case 0x68: c->l = c->b; break;	// LD L, B
	case 0x69: c->l = c->c; break;	// LD L, C
	case 0x6A: c->l = c->d; break;	// LD L, D
	case 0x6B: c->l = c->e; break;	// LD L, E
	case 0x6C: c->l = c->h; break;	// LD L, H
	case 0x6D: c->l = c->l; break;	// LD L, L
	case 0x6E: c->l = rb(c, get_hl(c)); break;	// LD L, (HL)
	case 0x6F: c->l = c->a; break;	// LD L, A

	case 0x70: wb(c, get_hl(c), c->b); break;	// LD (HL), B
	case 0x71: wb(c, get_hl(c), c->c); break;	// LD (HL), C
	case 0x72: wb(c, get_hl(c), c->d); break;	// LD (HL), D
	case 0x73: wb(c, get_hl(c), c->e); break;	// LD (HL), E
	case 0x74: wb(c, get_hl(c), c->h); break;	// LD (HL), H
	case 0x75: wb(c, get_hl(c), c->l); break;	// LD (HL), L
	case 0x77: wb(c, get_hl(c), c->a); break;	// LD (HL), A
	case 0x78: c->a = c->b; break;	// LD A, B
	case 0x79: c->a = c->c; break;	// LD A, C
	case 0x7A: c->a = c->d; break;	// LD A, D
	case 0x7B: c->a = c->e; break;	// LD A, E
	case 0x7C: c->a = c->h; break;	// LD A, H
	case 0x7D: c->a = c->l; break;	// LD A, L
	case 0x7E: c->a = rb(c, get_hl(c)); break;	// LD A, (HL)
	case 0x7F: c->a = c->a; break;	// LD A, A

	case 0xE0: wb(c, 0xFF00 | nextb(c), c->a); break;	// LDH (a8), A
	case 0xF0: c->a = rb(c, 0xFF00 | nextb(c)); break;	// LDH A, (a8)
	case 0xE2: wb(c, 0xFF00 | c->c, c->a); break;		// LD (C), A
	case 0xF2: c->a = rb(c, 0xFF00 | c->c); break;		// LD A, (C)
	case 0xEA: wb(c, nextw(c), c->a); break;			// LD (a16), A
	case 0xFA: c->a = rb(c, nextw(c)); break;			// LD A, (a16)

	case 0x01: set_bc(c, nextw(c)); break;				// LD BC, d16
	case 0x11: set_de(c, nextw(c)); break;				// LD DE, d16
	case 0x21: set_hl(c, nextw(c)); break;				// LD HL, d16
	case 0x31: c->sp = nextw(c); break;					// LD SP, d16

	case 0x08: ww(c, nextw(c), c->sp); break;	// LD (a16), SP
	case 0xF8: hlsp(c); break;					// LD HL, SP + r8
	case 0xF9: c->sp = get_hl(c); break;		// LD SP, HL

	case 0xC1: set_bc(c, pop(c)); break;	// POP BC
	case 0xD1: set_de(c, pop(c)); break;	// POP DE
	case 0xE1: set_hl(c, pop(c)); break;	// POP HL
	case 0xF1: pop_af(c); break;			// POP AF

	case 0xC5: push(c, get_bc(c)); break;	// PUSH BC
	case 0xD5: push(c, get_de(c)); break;	// PUSH DE
	case 0xE5: push(c, get_hl(c)); break;	// PUSH HL
	case 0xF5: push_af(c); break;			// PUSH AF

	case 0x04: c->b = inc(c, c->b); break;	// INC B
	case 0x14: c->d = inc(c, c->d); break;	// INC D
	case 0x24: c->h = inc(c, c->h); break;	// INC H
	case 0x34: wb(c, get_hl(c), inc(c, rb(c, get_hl(c)))); break;	// INC (HL)
	case 0x0C: c->c = inc(c, c->c); break;	// INC C
	case 0x1C: c->e = inc(c, c->e); break;	// INC E
	case 0x2C: c->l = inc(c, c->l); break;	// INC L
	case 0x3C: c->a = inc(c, c->a); break;	// INC A

	case 0x05: c->b = dec(c, c->b); break;	// DEC B
	case 0x15: c->d = dec(c, c->d); break;	// DEC D
	case 0x25: c->h = dec(c, c->h); break;	// DEC H
	case 0x35: wb(c, get_hl(c), dec(c, rb(c, get_hl(c)))); break;	// DEC (HL)
	case 0x0D: c->c = dec(c, c->c); break;	// DEC C
	case 0x1D: c->e = dec(c, c->e); break;	// DEC E
	case 0x2D: c->l = dec(c, c->l); break;	// DEC L
	case 0x3D: c->a = dec(c, c->a); break;	// DEC A

	case 0x27: daa(c); break; // DAA
	case 0x37: c->cf = 1; c->nf = 0; c->hf = 0; break; // SCF
	case 0x2F: c->a = ~c->a; c->nf = 1; c->hf = 1; break; // CPL
	case 0x3F: c->cf = !c->cf; c->nf = 0; c->hf = 0; break; // CCF

	case 0x80: c->a = add(c, c->b, 0); break;	// ADD B
	case 0x81: c->a = add(c, c->c, 0); break;	// ADD C
	case 0x82: c->a = add(c, c->d, 0); break;	// ADD D
	case 0x83: c->a = add(c, c->e, 0); break;	// ADD E
	case 0x84: c->a = add(c, c->h, 0); break;	// ADD H
	case 0x85: c->a = add(c, c->l, 0); break;	// ADD L
	case 0x86: c->a = add(c, rb(c, get_hl(c)), 0); break;	// ADD (HL)
	case 0x87: c->a = add(c, c->a, 0); break;	// ADD A
	case 0x88: c->a = add(c, c->b, c->cf); break; // ADC B
	case 0x89: c->a = add(c, c->c, c->cf); break; // ADC C
	case 0x8A: c->a = add(c, c->d, c->cf); break; // ADC D
	case 0x8B: c->a = add(c, c->e, c->cf); break; // ADC E
	case 0x8C: c->a = add(c, c->h, c->cf); break; // ADC H
	case 0x8D: c->a = add(c, c->l, c->cf); break; // ADC L
	case 0x8E: c->a = add(c, rb(c, get_hl(c)), c->cf); break; // ADC (HL)
	case 0x8F: c->a = add(c, c->a, c->cf); break; // ADC A

	case 0x90: c->a = sub(c, c->b, 0); break;	// SUB B
	case 0x91: c->a = sub(c, c->c, 0); break;	// SUB C
	case 0x92: c->a = sub(c, c->d, 0); break;	// SUB D
	case 0x93: c->a = sub(c, c->e, 0); break;	// SUB E
	case 0x94: c->a = sub(c, c->h, 0); break;	// SUB H
	case 0x95: c->a = sub(c, c->l, 0); break;	// SUB L
	case 0x96: c->a = sub(c, rb(c, get_hl(c)), 0); break;	// SUB (HL)
	case 0x97: c->a = sub(c, c->a, 0); break;	// SUB A
	case 0x98: c->a = sub(c, c->b, c->cf); break; // SBC B
	case 0x99: c->a = sub(c, c->c, c->cf); break; // SBC C
	case 0x9A: c->a = sub(c, c->d, c->cf); break; // SBC D
	case 0x9B: c->a = sub(c, c->e, c->cf); break; // SBC E
	case 0x9C: c->a = sub(c, c->h, c->cf); break; // SBC H
	case 0x9D: c->a = sub(c, c->l, c->cf); break; // SBC L
	case 0x9E: c->a = sub(c, rb(c, get_hl(c)), c->cf); break;	// SBC (HL)
	case 0x9F: c->a = sub(c, c->a, c->cf); break;	// SBC A

	case 0xA0: and (c, c->b); break;	// AND B
	case 0xA1: and (c, c->c); break;	// AND C
	case 0xA2: and (c, c->d); break;	// AND D
	case 0xA3: and (c, c->e); break;	// AND E
	case 0xA4: and (c, c->h); break;	// AND H
	case 0xA5: and (c, c->l); break;	// AND L
	case 0xA6: and (c, rb(c, get_hl(c))); break;	// AND (HL)
	case 0xA7: and (c, c->a); break;	// AND A
	case 0xA8: xor (c, c->b); break;	// XOR B
	case 0xA9: xor (c, c->c); break;	// XOR C
	case 0xAA: xor (c, c->d); break;	// XOR D
	case 0xAB: xor (c, c->e); break;	// XOR E
	case 0xAC: xor (c, c->h); break;	// XOR H
	case 0xAD: xor (c, c->l); break;	// XOR L
	case 0xAE: xor (c, rb(c, get_hl(c))); break;	// XOR (HL)
	case 0xAF: xor (c, c->a); break;	// XOR A

	case 0xB0: or(c, c->b); break;	// OR B
	case 0xB1: or(c, c->c); break;	// OR C
	case 0xB2: or(c, c->d); break;	// OR D
	case 0xB3: or(c, c->e); break;	// OR E
	case 0xB4: or(c, c->h); break;	// OR H
	case 0xB5: or(c, c->l); break;	// OR L
	case 0xB6: or(c, rb(c, get_hl(c))); break;	// OR (HL)
	case 0xB7: or(c, c->a); break;	// OR A
	case 0xB8: cmp(c, c->b); break;	// CP B
	case 0xB9: cmp(c, c->c); break;	// CP C
	case 0xBA: cmp(c, c->d); break;	// CP D
	case 0xBB: cmp(c, c->e); break;	// CP E
	case 0xBC: cmp(c, c->h); break;	// CP H
	case 0xBD: cmp(c, c->l); break;	// CP L
	case 0xBE: cmp(c, rb(c, get_hl(c))); break;	// CP (HL)
	case 0xBF: cmp(c, c->a); break;	// CP A
	
	case 0xC6: c->a = add(c, nextb(c), 0); break;		// ADD d8
	case 0xD6: c->a = sub(c, nextb(c), 0); break;		// SUB d8
	case 0xE6: and(c, nextb(c)); break;			// AND d8
	case 0xF6: or (c, nextb(c)); break;			// OR d8
	case 0xCE: c->a = add(c, nextb(c), c->cf); break;	// ADC d8
	case 0xDE: c->a = sub(c, nextb(c), c->cf); break;	// SBC d8
	case 0xEE: xor(c, nextb(c)); break;			// XOR d8
	case 0xFE: cmp(c, nextb(c)); break;			// CP d8

	case 0x03: set_bc(c, get_bc(c) + 1); break;	// INC BC
	case 0x13: set_de(c, get_de(c) + 1); break;	// INC DE
	case 0x23: set_hl(c, get_hl(c) + 1); break;	// INC HL
	case 0x33: c->sp += 1; break;
	case 0x0B: set_bc(c, get_bc(c) - 1); break;
	case 0x1B: set_de(c, get_de(c) - 1); break;
	case 0x2B: set_hl(c, get_hl(c) - 1); break;
	case 0x3B: c->sp -= 1; break;

	case 0x09: addhl(c, get_bc(c)); break;	// ADD HL, BC
	case 0x19: addhl(c, get_de(c)); break;	// ADD HL, DE
	case 0x29: addhl(c, get_hl(c)); break;	// ADD HL, HL
	case 0x39: addhl(c, c->sp); break;		// ADD HL, SP
	case 0xE8: ldsp(c); break;				// ADD SP, r8

	case 0x18: jr(c, nextb(c)); break;			// JR r8
	case 0x20: jr_cond(c, c->zf == 0); break;	// JR NZ, r8
	case 0x28: jr_cond(c, c->zf == 1); break;	// JR Z, r8
	case 0x30: jr_cond(c, c->cf == 0); break;	// JR NC, r8
	case 0x38: jr_cond(c, c->cf == 1); break;	// JR C, r8

	case 0xC3: jp(c, nextw(c)); break;			// JP, a16
	case 0xE9: jp(c, get_hl(c)); break;
	case 0xC2: jp_cond(c, c->zf == 0); break;	// JP NZ, a16
	case 0xCA: jp_cond(c, c->zf == 1); break;	// JP Z, a16
	case 0xD2: jp_cond(c, c->cf == 0); break;	// JP NC, a16
	case 0xDA: jp_cond(c, c->cf == 1); break;	// JP C, a16

	case 0xCD: call(c, nextw(c)); break;		// CALL a16
	case 0xC4: call_cond(c, c->zf == 0); break; // CALL NZ, a16
	case 0xCC: call_cond(c, c->zf == 1); break; // CALL Z, a16
	case 0xD4: call_cond(c, c->cf == 0); break; // CALL NC, a16
	case 0xDC: call_cond(c, c->cf == 1); break; // CALL C, a16

	case 0xC9: ret(c); break;					// RET
	case 0xD9: c->iff = 1; ret(c); break;		// RETI
	case 0xC0: ret_cond(c, c->zf == 0); break;	// RET NZ
	case 0xC8: ret_cond(c, c->zf == 1); break;	// RET Z
	case 0xD0: ret_cond(c, c->cf == 0); break;	// RET NC
	case 0xD8: ret_cond(c, c->cf == 1); break;	// RET C

	case 0xC7: call(c, 0x00); break; // RST 00
	case 0xD7: call(c, 0x10); break; // RST 10
	case 0xE7: call(c, 0x20); break; // RST 20
	case 0xF7: call(c, 0x30); break; // RST 30
	case 0xCF: call(c, 0x08); break; // RST 08
	case 0xDF: call(c, 0x18); break; // RST 18
	case 0xEF: call(c, 0x28); break; // RST 28
	case 0xFF: call(c, 0x38); break; // RST 38

	case 0x07: rlca(c); break;	// RLCA
	case 0x17: rla(c); break;	// RLA
	case 0x0F: rrca(c); break;	// RRCA
	case 0x1F: rra(c); break;	// RRA
	
	case 0xCB: execute_opcode_cb(c, nextb(c)); break;	// CB 7C

	default: printf("unrecognized opcode: %02X\n", opcode); return 1;
	}

	return 0;
}

static inline void draw_scanline(g80* const c, byte scanline)
{
	byte control = rb(c, 0xFF40);

	word bg_tile_map = ((control & 0x08) == 0x08) ? 0x9C00 : 0x9800;
	word wn_tile_map = ((control & 0x40) == 0x40) ? 0x9C00 : 0x9800;
	word tile_data = ((control & 0x10) == 0x10) ? 0x8000 : 0x8800;

	byte scroll_x = rb(c, 0xFF43);
	byte scroll_y = rb(c, 0xFF42);
	byte window_y = rb(c, 0xFF4A);
	byte window_x = rb(c, 0xFF4B) - 7;
	byte palette  = rb(c, 0xFF47);

	bool window = ((control & 0x20) == 0x20);

	byte bg_y = scanline + scroll_y;
	byte wn_y = c->internal_scanline - window_y;

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

		int16_t bg_tile_index = ((control & 0x10) == 0x10) ? (byte)rb(c, bg_tile_addr) : (int8_t)rb(c, bg_tile_addr);
		int16_t wn_tile_index = ((control & 0x10) == 0x10) ? (byte)rb(c, wn_tile_addr) : (int8_t)rb(c, wn_tile_addr);

		word bg_tile_loc = ((control & 0x10) == 0x10) ? tile_data + bg_tile_index * 16 : tile_data + (bg_tile_index + 128) * 16;
		word wn_tile_loc = ((control & 0x10) == 0x10) ? tile_data + wn_tile_index * 16 : tile_data + (wn_tile_index + 128) * 16;

		byte bg_line = (bg_y % 8) * 2;
		byte wn_line = (wn_y % 8) * 2;

		byte bg_lo = rb(c, bg_tile_loc + bg_line);
		byte bg_hi = rb(c, bg_tile_loc + bg_line + 1);

		byte wn_lo = rb(c, wn_tile_loc + wn_line);
		byte wn_hi = rb(c, wn_tile_loc + wn_line + 1);

		byte bg_n = 7 - (bg_x % 8);
		byte wn_n = 7 - (wn_x % 8);

		byte bg_color = ((bg_lo & (1 << bg_n)) != 0) | (((bg_hi & (1 << bg_n)) != 0) << 1);
		byte wn_color = ((wn_lo & (1 << wn_n)) != 0) | (((wn_hi & (1 << wn_n)) != 0) << 1);

		c->bg_buffer[px + scanline * SCREEN_WIDTH] = c->colors[(palette >> (bg_color * 2)) & 3];
		c->wn_buffer[px + scanline * SCREEN_WIDTH] = c->colors[(palette >> (wn_color * 2)) & 3];

		if ((control & 0x1) == 0x1)
		{
			c->screen_buffer[px + scanline * SCREEN_WIDTH] = (window && scanline >= window_y && px >= window_x) ? c->wn_buffer[px + scanline * SCREEN_WIDTH] : c->bg_buffer[px + scanline * SCREEN_WIDTH];
		}
		else c->screen_buffer[px + scanline * SCREEN_WIDTH] = c->colors[0];
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

			byte yPos = rb(c, oam_addr++) - 16;
			byte xPos = rb(c, oam_addr++) - 8;
			byte tile_index = rb(c, oam_addr++);
			byte attributes = rb(c, oam_addr++);

			palette = ((attributes & 0x10) == 0x10) ? rb(c, 0xFF49) : rb(c, 0xFF48);

			if (scanline < yPos || scanline >= yPos + height) continue;

			bool yFlip = (attributes & 0x40) == 0x40;
			bool xFlip = (attributes & 0x20) == 0x20;

			byte line = (yFlip) ? (height - scanline + yPos - 1) * 2 : (scanline - yPos) * 2;

			if (height == 16)
			{
				tile_index &= 0xFE;
			}

			byte lo = rb(c, 0x8000 + tile_index * 16 + line);
			byte hi = rb(c, 0x8000 + tile_index * 16 + line + 1);

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

					if ((attributes & 0x80) == 0 || c->screen_buffer[xPos + px + scanline * SCREEN_WIDTH] == c->colors[rb(c, 0xFF47) & 3])
					{
						c->screen_buffer[xPos + px + scanline * SCREEN_WIDTH] = c->colors[palette >> (color * 2) & 3];
					}
				}
			}

			xPositions[count] = xPos;

			count++;
		}
	}
}

static inline void update_lcdstatus(g80* const c)
{
	byte status = rb(c, 0xFF41);
	byte scanline = rb(c, 0xFF44);
	byte mode = status & 3;

	byte next_mode = 0;
	bool need_int = false;

	if (scanline >= 144)
	{
		next_mode = 1;
		if (next_mode != mode)
		{
			c->internal_scanline = 0;
			wb(c, 0xFF0F, rb(c, 0xFF0F) | 0x1);
		}

		need_int = ((status & 0x10) == 0x10);
	}
	else
	{
		if (c->scanline_counter >= 456 - 80)
		{
			next_mode = 2;
			need_int = ((status & 0x20) == 0x20);
		}
		else if (c->scanline_counter >= 456 - 80 - 172)
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
;					draw_scanline(c, scanline);
				}
			}

			need_int = ((status & 0x08) == 0x08);
		}
	}

	if (need_int && (mode != next_mode))
	{
		wb(c, 0xFF0F, rb(c, 0xFF0F) | 0x2);
	}

	if (scanline == rb(c, 0xFF45))
	{
		if ((status & 0x04) == 0)
		{
			status |= 0x4;
			if ((status & 0x40) == 0x40)
			{
				wb(c, 0xFF0F, rb(c, 0xFF0F) | 0x2);
			}
		}
	}
	else status &= ~0x4;

	status &= ~3;
	status |= next_mode;

	wb(c, 0xFF41, status);
}

void g80_init(g80* const c)
{
	c->userdata = c;
	c->read_byte = NULL;
	c->write_byte = NULL;

	c->a = 0x01;
	c->b = 0x00;
	c->c = 0x13;
	c->d = 0x00;
	c->e = 0xD8;
	c->h = 0x01;
	c->l = 0x4D;

	c->zf = 1;
	c->nf = 0;
	c->hf = 1;
	c->cf = 1;

	c->iff = 0;
	c->interrupt_delay = 0;
	c->halted = 0;

	c->pc = 0x0100;
	c->sp = 0xFFFE;

	c->cyc = 0;
	c->scanline_counter = 456;
	c->divider_counter = 0;
	c->timer_counter = 0;

	c->colors[0] = 0xff58d0d0;
	c->colors[1] = 0xff40a8a0;
	c->colors[2] = 0xff288070;
	c->colors[3] = 0xff105040;

	c->internal_scanline = 0;
	c->div = 0;
}

void process_interrupts(g80* const c)
{
	if (c->interrupt_delay > 0)
	{
		c->interrupt_delay -= 1;
		if (c->interrupt_delay == 0)
		{
			c->iff = 1;
		}
		return;
	}

	byte IF = rb(c, 0xFF0F);
	byte IE = rb(c, 0xFFFF);

	if ((IF & IE) != 0)
	{
		if (c->halted)
		{
			c->halted = 0;
			// c->cyc += 4;
		}
		if (c->iff)
		{
			c->iff = 0;
			c->cyc += 20;
			if ((IF & 0x01) == 0x01)
			{
				// VBLANK
				IF &= ~0x01;
				push(c, c->pc);
				c->pc = 0x0040;
			}

			else if ((IF & 0x02) == 0x02)
			{
				// LCD
				IF &= ~0x02;
				push(c, c->pc);
				c->pc = 0x0048;
			}

			else if ((IF & 0x04) == 0x04)
			{
				// TIMER
				IF &= ~0x04;
				push(c, c->pc);
				c->pc = 0x0050;
			}

			else if ((IF & 0x08) == 0x08)
			{
				// SERIAL
				IF &= ~0x08;
				push(c, c->pc);
				c->pc = 0x0058;
			}

			else if ((IF & 0x10) == 0x10)
			{
				// JOYPAD
				IF &= ~0x10;
				push(c, c->pc);
				c->pc = 0x0060;
			}

			wb(c, 0xFF0F, IF);
		}
	}
}

static int timer_freq[4] = { 1024, 16, 64, 256 };

void timer_step(g80* const c, int elapsed)
{
	c->divider_counter += elapsed;

	if (c->divider_counter >= 256)
	{
		c->divider_counter = 0;
		c->div++;
	}

	byte tc = rb(c, 0xFF07);

	if ((tc & 0x4) == 0x4)
	{
		c->timer_counter += elapsed;

		while (c->timer_counter >= timer_freq[tc & 3])
		{
			c->timer_counter = 0;

			byte timer = rb(c, 0xFF05);

			if (timer == 0xFF)
			{
				wb(c, 0xFF0F, rb(c, 0xFF0F) | 0x4);
				wb(c, 0xFF05, rb(c, 0xFF06));
			}
			else
			{
				wb(c, 0xFF05, timer + 1);
			}
		}
	}
}

void ppu_step(g80* const c, int elapsed)
{
	c->scanline_counter -= elapsed;

	// update_lcdstatus(c);

	if (c->scanline_counter <= 0)
	{
		byte scanline = rb(c, 0xFF44);

		wb(c, 0xFF44, ++scanline);

		if (scanline > 153)
		{
			wb(c, 0xFF44, 0);
		}

		c->scanline_counter = 456;

		if ((rb(c, 0xFF40) & 0x20) == 0x20 && rb(c, 0xFF4A) >= 0 && rb(c, 0xFF4A) < 144 && rb(c, 0xFF4B) >= 0 && rb(c, 0xFF4B) < 167)
		{
			if (c->internal_scanline == 0)
			{
				c->internal_scanline = scanline;
			}
			else if (c->internal_scanline != scanline)
			{
				c->internal_scanline++;
			}
			else
			{
				c->internal_scanline = scanline;
			}


			if (c->internal_scanline > 153) c->internal_scanline = 0;
		}
	}

	update_lcdstatus(c);
}

int g80_step(g80* const c)
{
	if (c->halted)
	{
		execute_opcode(c, 0x00);
	}
	else
	{
		execute_opcode(c, nextb(c));
	}

	return 0;
}