// very simple history buffer for trace printfs that can be dumped when a trigger is hit

#include <sys/types.h>

#include "boot.h"
#include "misc.h"
#include "net.h"
#include "sim.h"
#include "5370.h"
#include "bus.h"
#include "hpib.h"
#include "front_panel.h"
#include "chip.h"

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include "6800.h"
#include "6800.cc.h"

#if defined(DEBUG) || defined(INSN_TRACE)

char *old_deco[N_OP] = {
/* 0 */	"x00", "nop", "x02", "x03", "x04", "x05", "tap", "tpa", "inx", "dex", "clv", "sev", "clc", "sec", "cli", "sei",
/* 1 */	"sba", "cba", "x12", "x13", "x14", "x15", "tab", "tba", "x18", "daa", "x1a", "aba", "x1c", "x1d", "x1e", "x1f",
/* 2 */	"bra", "x21", "bhi", "bls", "bcc", "bcs", "bne", "beq", "bvc", "bvs", "bpl", "bmi", "bge", "blt", "bgt", "ble",
/* 3 */	"tsx", "ins", "pul-a", "pul-b", "des", "txs", "psh-a", "psh-b", "x38", "rts", "x3a", "rti", "x3c", "x3d", "wai", "swi",
/* 4 */	"neg-a", "x41", "x42", "com-a", "lsr-a", "x45", "ror-a", "asr-a", "asl-a", "rol-a", "dec-a", "x4b", "inc-a", "tst-a", "x4e", "clr-a",
/* 5 */	"neg-b", "x51", "x52", "com-b", "lsr-b", "x55", "ror-b", "asr-b", "asl-b", "rol-b", "dec-b", "x5b", "inc-b", "tst-b", "x5e", "clr-b",
/* 6 */	"neg-x@", "x61", "x62", "com-x@", "lsr-x@", "x65", "ror-x@", "asr-x@", "asl-x@", "rol-x@", "dec-x@", "x6b", "inc-x@", "tst-x@", "jmp-x@", "clr-x@",
/* 7 */	"neg-##", "x71", "x72", "com-##", "lsr-##", "x75", "ror-##", "asr-##", "asl-##", "rol-##", "dec-##", "x7b", "inc-##", "tst-##", "jmp-##", "clr-##",
/* 8 */	"sub-a#", "cmp-a#", "sbc-a#", "x83", "and-a#", "bit-a#", "lda-a#", "x87", "eor-a#", "adc-a#", "ora-a#", "add-a#", "cpx-#", "bsr", "lds-#", "x8f",
/* 9 */	"sub-a@", "cmp-a@", "sbc-a@", "x93", "and-a@", "bit-a@", "lda-a@", "sta-a@", "eor-a@", "adc-a@", "ora-a@", "add-a@", "cpx-@", "x9d", "lds-@", "sts-@",
/* a */	"sub-ax@", "cmp-ax@", "sbc-ax@", "xa3", "and-ax@", "bit-ax@", "lda-ax@", "sta-ax@", "eor-ax@", "adc-ax@", "ora-ax@", "add-ax@", "cpx-x@", "jsr-x@", "lds-x@", "sts-x@",
/* b */	"sub-a##", "cmp-a##", "sbc-a##", "xb3", "and-a##", "bit-a##", "lda-a##", "sta-a##", "eor-a##", "adc-a##", "ora-a##", "add-a##", "cpx-##", "jsr-##", "lds-##", "sts-##",
/* c */	"sub-b#", "cmp-b#", "sbc-b#", "xc3", "and-b#", "bit-b#", "lda-b#", "xc7", "eor-b#", "adc-b#", "ora-b#", "add-b#", "xcc", "xcd", "ldx-#", "xcf",
/* d */	"sub-b@", "cmp-b@", "sbc-b@", "xd3", "and-b@", "bit-b@", "lda-b@", "sta-b@", "eor-b@", "adc-b@", "ora-b@", "add-b@", "xdc", "xdd", "ldx-@", "stx-@",
/* e */	"sub-bx@", "cmp-bx@", "sbc-bx@", "xe3", "and-bx@", "bit-bx@", "lda-bx@", "sta-bx@", "eor-bx@", "adc-bx@", "ora-bx@", "add-bx@", "xec", "xed", "ldx-x@", "stx-x@",
/* f */	"sub-b##", "cmp-b##", "sbc-b##", "xf3", "and-b##", "bit-b##", "lda-b##", "sta-b##", "eor-b##", "adc-b##", "ora-b##", "add-b##", "xfc", "xfd", "ldx-##", "stx-##",
};

char *deco[N_OP] = {
/* 0 */	"x00", "nop", "x02", "x03", "x04", "x05", "CC = a", "a = CC", "x++", "x--", "V=0", "V=1", "C=0", "C=1", "I=0", "I=1",
/* 1 */	"a = a-b", " (a == b)?", "x12", "x13", "x14", "x15", "a = b", "b = a", "x18", "daa", "x1a", "a = a+b", "x1c", "x1d", "x1e", "x1f",
/* 2 */	"bra", "x21", "bhi", "bls", "bcc", "bcs", "bne", "beq", "bvc", "bvs", "bpl", "bmi", "bge", "blt", "bgt", "ble",
/* 3 */	"x = sp+1", "sp++", "a = pop", "b = pop", "sp--", "sp = x--", "push a", "push b", "x38", "rts", "x3a", "rti", "x3c", "x3d", "wai", "swi",
/* 4 */	"a = -a", "x41", "x42", "a = ~a", "a >l>", "x45", "a >r>", "a >a>", "a <a<", "a <r<", "a--", "x4b", "a++", "a ==?", "x4e", "a = 0",
/* 5 */	"b = -b", "x51", "x52", "b = ~b", "b >l>", "x55", "b >r>", "b >a>", "b <a<", "b <r<", "b--", "x5b", "b++", "b ==?", "x5e", "b = 0",
/* 6 */	"-=", "x61", "x62", "~=", ">l>", "x65", ">r>", ">a>", "<a<", "<r<", "--", "x6b", "++", "==?", "jmp", "= 0",
/* 7 */	"-=", "x71", "x72", "~=", ">l>", "x75", ">r>", ">a>", "<a<", "<r<", "--", "x7b", "++", "==?", "jmp", "= 0",
/* 8 */	"a -=", "a ==", "a -=C", "x83", "a &=", "a &", "a =", "x87", "a ^=", "a +=C", "a |=", "a +=", "x ==", "bsr", "sp =", "x8f",
/* 9 */	"a -=", "a ==", "a -=C", "x93", "a &=", "a &", "a =", "= a", "a ^=", "a +=C", "a |=", "a +=", "x ==", "x9d", "sp =", "= sp",
/* a */	"a -=", "a ==", "a -=C", "xa3", "a &=", "a &", "a =", "= a", "a ^=", "a +=C", "a |=", "a +=", "x ==", "jsr", "sp =", "= sp",
/* b */	"a -=", "a ==", "a -=C", "xb3", "a &=", "a &", "a =", "= a", "a ^=", "a +=C", "a |=", "a +=", "x ==", "jsr", "sp =", "= sp",
/* c */	"b -=", "b ==", "b -=C", "xc3", "b &=", "b &", "b =", "xc7", "b ^=", "b +=C", "b |=", "b +=", "xcc", "xcd", "x =", "xcf",
/* d */	"b -=", "b ==", "b -=C", "xd3", "b &=", "b &", "b =", "= b", "b ^=", "b +=C", "b |=", "b +=", "xdc", "xdd", "x =", "= x",
/* e */	"b -=", "b ==", "b -=C", "xe3", "b &=", "b &", "b =", "= b", "b ^=", "b +=C", "b |=", "b +=", "xec", "xed", "x =", "= x",
/* f */	"b -=", "b ==", "b -=C", "xf3", "b &=", "b &", "b =", "= b", "b ^=", "b +=C", "b |=", "b +=", "xfc", "xfd", "x =", "= x",
};

// most number of opcodes seen after instrument use, including (simple) HPIB, = 136
// 6800 spec says 197 possible, so we've not interpreted (197 - 136) = 61 of them
// output from the '@' command:

//opcode coverage 136/197 61
//not-interp: sba bhi bls tsx ins des txs wai asr-b neg-x@ com-x@ asl-x@ neg-## com-## ror-## asr-## asl-## rol-## sbc-a#
//bit-a@ eor-a@ lds-@ sts-@ sub-ax@ sbc-ax@ and-ax@ bit-ax@ eor-ax@ ora-ax@ cpx-x@ lds-x@ sts-x@ sbc-a## and-a## bit-a## eor-a## adc-a## ora-a## cpx-## lds-## sts-##
//sub-b# sbc-b# cmp-b@ bit-b@ ora-b@ sub-bx@ and-bx@ bit-bx@ eor-bx@ adc-bx@ add-bx@ stx-x@ sub-b## cmp-b## sbc-b## and-b## bit-b## adc-b## ora-b## add-b## 
//61 remaining


#define	NTRB	1024	// how many insns in trace buffer
static char trbuf[NTRB][256];
static int trbi = 0;

void trace_dump()
{
	int i;
	
	for (i = trbi+1; i != trbi;) {
		printf("%s", &trbuf[i][0]);
		trbuf[i][0] = 0;
		i++;
		if (i >= NTRB) i = 0;
	}
}

void trace_clear()
{
	int i;
	
	for (i = 0; i < NTRB; i++)
		trbuf[i][0] = 0;
	trbi = 0;
}

void trace_on()
{
	iTrace = 1;
	trace_clear();
}

static char *last_s;

char *tr_s()
{
	// if the last use didn't end with a newline allow the string to be appended to
	if (last_s && (*(last_s + strlen(last_s) - 1) != '\n')) {
		last_s += strlen(last_s);
	} else {
		last_s = &trbuf[trbi++][0];
		if (trbi >= NTRB) trbi=0;
	}
	
	return last_s;
}

void tr_printf(int ignore)
{
	if (!iTrace || iSnap || iDump) {
		printf("%s", last_s);
	}
}


// build a table of attributes for each opcode via (gross misuse of) insn set definition

// for data-flow analysis
#define inc(p8) 0;
#define dec(p8) 0;
#define add(r_dst, r_src) 0;	f |= _LD | _ ## r_dst;
#define adc(r_dst, r_src) 0;	f |= _LD | _ ## r_dst;
#define sub(r_dst, r_src) 0;	f |= _LD | _ ## r_dst;
#define sbc(r_dst, r_src) 0;	f |= _LD | _ ## r_dst;
#define daa(reg)
#define cmp(p8_1, p8_2)
#define cmp16(p16_1, p16_2)
#define lsr(p8) 0;
#define asr(p8) 0;
#define asl(p8) 0;
#define ror(p8) 0;
#define rol(p8) 0;
#define eor(r_dst, r_src) 0;	f |= _DFLOW | _XOR | _ ## r_dst;
#define or(r_dst, r_src) 0;		f |= _DFLOW | _OR  | _ ## r_dst;
#define and(r_dst, r_src) 0;	f |= _DFLOW | _AND | _ ## r_dst;
#define bit(r_dst, r_src)		f |= _DFLOW | _BIT | _ ## r_dst;
#define com(p8) 0;
#define neg(p8) 0;
#define tst(p8)
#define clr() 0;
#define xfer(dst, src)
#define ld(reg)					f |= _LD | _ ## reg;
#define ld16(reg)
#define st(reg)
#define st16(reg)
#define pushB(p8)
#define pushW(p16)
#define pullB()
#define pullW()
#define branch()
#define computed_goto(addr)

#define _08_REL()	f |= _REL;
#define _08_IMM()	f |= _IMM;
#define _16_WIM()	f |= _WIM;
#define _08_DIR()	f |= _DIR;
#define _16_EXT()	f |= _EXT;
#define _08_IDX()	f |= _IDX;

#define	SF(flags)	f |= flags;

#define NA(x)
#define BRANCH(x)	SF(_BRANCH)
#define COND(x)		SF(_COND)
#define CALL(x)		SF(_CALL)
#define RTN(x)		SF(_RTN)
#define	WR_A(x)		SF(_LD | _rA)
#define	WR_B(x)		SF(_LD | _rB)
#define IMM(x)		x; SF(0)
#define R8(x)		x; SF(_R | _8 | _R8)
#define W8(x)		x; SF(_W | _8 | _W8)
#define RMW_8(x)	x; SF(_R | _RMW | _8 | _R8)
#define R16(x)		x; SF(_R | _16 | _R16)
#define W16(x)		x; SF(_W | _16 | _W16)

#define INSN(opc, opn, x) 	 z = &insn_attrs[opc]; f=0; x; z->flags = f;

static insn_attrs_t insn_attrs[N_OP];

void trace_init()
{
	int i;
	u1_t opcode;
	insn_attrs_t *z;
	u4_t f;
	
	// dummies for macro expansion
	u1_t rA, rB, tU8;
	
	#include "6800.insns.h"

	for (i=0; i<256; i++) {
		z = &insn_attrs[i];
		//printf("%02x %08x\n", i, z->flags);
		//printf("_WIM opcodes: ");
		//if (insn_attrs[i].flags & _WIM) printf("%02x ", i); // check that it really works
	}
	//printf("\n");
}

typedef enum { W_RAM, W_READ, W_WRITE, W_LABEL, W_COMMENT, W_SKIP_RTN, W_SKIP_RANGE, W_END } what_e;
char *space[] = { "ram", "dev", "dev" };

typedef struct {
	u2_t addr, addr2;
	u2_t a_type;
	u1_t rw;
	what_e type;
	char *name;
} what_t;

#define	RAM(a, n)				{ a, 0, 0, 0, W_RAM, n }
#define	RAM_TYPE_R(a, r, n)		{ a, 0, r, 1, W_RAM, "(" #r " read) " n }
#define	RAM_TYPE_W(a, r, n)		{ a, 0, r, 0, W_RAM, "(" #r " read) " n }
#define	READ(a)					{ a, 0, 0, 0, W_READ, #a }
#define	WRITE(a)				{ a, 0, 0, 0, W_WRITE, #a }
#define	RANGE_R(a, a2, n)		{ a, a2, 0, 0, W_READ, n }
#define	RANGE_W(a, a2, n)		{ a, a2, 0, 0, W_WRITE, n }
#define LABEL(a, n)				{ a, 0, 0, 0, W_LABEL, n }
#define COMMENT(a, n)			{ a, 0, 0, 0, W_COMMENT, n }
#define SKIP_RTN(a, n)			{ a, 0, 0, 0, W_SKIP_RTN, n }
#define SKIP_RANGE(a, a2, n)	{ a, a2, 0, 0, W_SKIP_RANGE, n }

static what_t what[] = {

	// display
	READ(RREG_KEY_SCAN),
	RANGE_W(ADDR_LEDS(12), ADDR_LEDS(0), "LEDs"),	// remember: reversed LED addressing
	RANGE_W(ADDR_7SEG(0), ADDR_7SEG(15), "7-seg display"),

	// arming
	READ(RREG_LDACSR),
	READ(RREG_A16_SWITCHES),
	READ(RREG_I1),
	READ(RREG_ST),
	READ(RREG_N0ST),
	READ(RREG_N0H),
	READ(RREG_N0L),
	READ(RREG_N1N2H),
	READ(RREG_N1N2L),
	READ(RREG_N3H),
	READ(RREG_N3L),
	
	WRITE(WREG_LDACCW),
	WRITE(WREG_LDACSTART),
	WRITE(WREG_LDACSTOP),
	WRITE(WREG_O2),
	WRITE(WREG_O1),
	WRITE(WREG_O3),
	
	// HPIB
	READ(R0_data_in),
	READ(R1_state),
	READ(R2_state),
	READ(R3_status),

	WRITE(W0_data_out-4),
	WRITE(W1_status-4),
	WRITE(W2_ctrl-4),
	
	// RAM
	RAM(0xb1, "accum trg lvl"),
	RAM(0xb8, "DAC start"),
	RAM(0xb9, "DAC stop"),
	RAM(0xb4, "N0 overflows counted H"),
	RAM(0xb5, "N0 overflows counted L"),

	RAM_TYPE_W(0xc2, WREG_O1, "template"),
	RAM_TYPE_W(0xc3, WREG_O2, "template"),

	RAM(0xc5, "N0 ovfl com L"),
	RAM(0xc9, "N0 ovfl tmp H"),
	RAM(0xca, "N0 ovfl tmp L"),
	RAM(0xcb, "N0-1-2 tmp H"),
	RAM(0xcc, "N0-1-2 tmp L"),
	
	// ce-e1 zeroed buffer by @6c6a
	
	RAM(0xe4, "N3 overflows counted"),
	RAM(0xeb, "gate time H"),
	RAM(0xec, "gate time L"),
	
	// ROM
	LABEL(0x76f6, "B = @b1 * 10"),
	LABEL(0x620b, "add RREG_ST bits and write HPIB status"),

	COMMENT(0x6b9d, "inactive(O2_ARM_MODE) i.e. armed with start/stop"),
	COMMENT(0x6b9f, "active(O2_ARM_EN)"),
	COMMENT(0x6ba3, "inactive(O1_HSET2|O1_HSET1|O1_HSTD)"),
	COMMENT(0x6ba5, "active(O1_HSET1|O1_HSTD) makes a pulse with above?"),

	SKIP_RANGE(0x6064, 0x606e, "x*64 delay loop"),
	
	{ 0, 0, 0, 0, W_END, "" }
};

void trace_opr(u4_t f, u1_t b2, u2_t x, u2_t tAddr, u1_t tU8, u2_t tU16)
{
	if (f & _WIM) {
			PF(" #%04x", tU16);
	} else
	
	if (f & _IMM) {
			PF(" #%02x", tU8);
	} else
	
	{
		if (f & _R8)  PF(" %02x", tU8);
		if (f & _R16) PF(" %04x", tU16);
		if (f & _W8)  PF(" %02x", tU8);
		if (f & _W16) PF(" %04x", tU16);
	
		if (f & _DIR) {
			PF("@%02x", tAddr);
		}
		
		if (f & _EXT) {
			PF("@%02x", tAddr);		// print 16-bit address as 2 digits and let it auto expand
		}
		
		if (f & _IDX) {
			PF("@%02x(x+%02x)", (u2_t) tAddr, b2);		// print 16-bit address as 2 digits and let it auto expand
		}
	}
}

typedef struct {
	u4_t flags, flags2;
	what_t *what;
} b_target_t;

static b_target_t branch_target[ROM_SIZE];

static u2_t tA, tB;
static u1_t rwA, rwB;
static bool skipping = FALSE;

void trace(u4_t iCount, u2_t rPC, u2_t nPC, u1_t taken, u1_t IRQ, u1_t IRQs, u1_t a, u1_t b, u2_t x, u2_t sp, u1_t C, u1_t VNZ, u1_t tU8, u2_t tU16)
{
	int i, store, imm;
	u4_t f, bt;
	u1_t opcode, b2, b3;
	u2_t tAddr;
	s2_t tS16;
	u1_t *rom = &rom_image[0] - ROM_START;
	u1_t *ram = &ram_image[0] - RAM_START;
	b_target_t *tgt = &branch_target[0] - ROM_START, *tp;
	what_t *w;
	insn_attrs_t *ia;
	
	opcode = rom[rPC];
	ia = &insn_attrs[opcode];
	f = ia->flags;
	store = f & _W;
	b2 = rom[rPC+1];
	b3 = rom[rPC+2];
	tp = &tgt[rPC];
	bt = tp->flags | tp->flags2;
	
	// skip delay loops, etc.

	static int skip_init;
	if (!skip_init) {
		skip_init=1;
		for (i=0; w=&what[i], w->type != W_END; i++) {
			if (w->type == W_SKIP_RANGE) {
				tgt[w->addr].what = w;
				tgt[w->addr].flags2 |= _SKIP_START;
				tgt[w->addr2].what = w;
				tgt[w->addr2].flags2 |= _SKIP_STOP;
			}
		}
	}
	
	if (bt & _SKIP_START) {
		PF("skip start %04x %s\n", rPC, tp->what->name);
		skipping = TRUE;
	}
	
	if (bt & _SKIP_STOP) {
		PF("skip stop  %04x %s\n", rPC, tp->what->name);
		skipping = FALSE;
	}
	
	if (skipping) return;
	
	if (bt) {
		for (i=0; w=&what[i], w->type != W_END; i++) {
			if (w->type == W_LABEL && w->addr == rPC) {
				PF("%s:\n", w->name);
				break;
			}
		}
		
		if (w->type == W_END) {
			if (bt & (_BRANCH | _COND)) PF("branch target:\n");
			if (bt & _CALL) PF("call target:\n");
		}
	}
	
	tAddr = 0xffff, w=0;
	
	if (f & _WIM) tU16 = (b2 << 8) | b3; else
	if (f & _IMM) tU8 = b2; else
	if (f & _REL) tS16 = (s2_t) (s1_t) b2; else
	if (f & _DIR) tAddr = (u2_t) b2; else
	if (f & _EXT) tAddr = (b2 << 8) | b3; else
	if (f & _IDX) tAddr = x + (u2_t) b2;
	
	if (tAddr != 0xffff) {
		for (i=0; w=&what[i], w->type != W_END; i++) {
			if (((w->type == W_RAM) || ((w->type == W_READ) && !store) || ((w->type == W_WRITE) && store)) &&
				((!w->addr2 && w->addr == tAddr) || (tAddr >= w->addr && tAddr <= w->addr2))) {
				break;
			}
		}
	}

	if (f & _LD) {
		if (f & _DIR) {
			if (f & _rA) {
				if (w && w->a_type) {
					tA = w->a_type;
					rwA = w->rw;
				} else {
					tA = tAddr;
					rwA = (f & _R)? 1:0;
				}
			}
			
			if (f & _rB) {
				if (w && w->a_type) {
					tB = w->a_type;
					rwB = w->rw;
				} else {
					tB = tAddr;
					rwB = (f & _R)? 1:0;
				}
			}
		} else {
			if (f & _rA) tA = 0;
			if (f & _rB) tB = 0;
		}
	}

	PF("%d %04x %c%c%c %02x-%02x-%02x At%02x Bt%02x SP=%04x %c%c%c%c X=%04x A=%02x B=%02x\t",
		iCount, rPC, (IRQs & INT_IRQ)? 'I':'_', (IRQ & INT_NMI)? 'N':'_', IRQ? 'M':'_', opcode, b2, b3, tA, tB, sp,
		getC()?'C':'c', getV()?'V':'v', getN()?'N':'n', getZ()?'Z':'z', x, a, b);

	if (f & _W) {
		trace_opr(f, b2, x, tAddr, tU8, tU16);
		PF(" %s\t", deco[opcode]);
	} else
	
	if (f & (_BRANCH | _CALL | _RTN)) {
		if (f & _IDX) PF(" %s %04x(x+%02x)\t", deco[opcode], nPC, b2); else PF(" %s %04x\t", deco[opcode], nPC);
		tgt[nPC].flags = f;	// where we actually went
	} else
	
	if (f & _COND) {
		u2_t cPC = rPC + 2 + tS16;
		PF(" %s %04x%s\t", deco[opcode], cPC, ((f & _COND) && taken)? "(taken)":"");
		tgt[cPC].flags = f;	// where we would go
		tgt[nPC].flags = f;	// where we actually went
	} else
	
	{
		// _R, _RMW or immediate
		PF(" %s", deco[opcode]);
		trace_opr(f, b2, x, tAddr, tU8, tU16);
		PF("\t");
	}
	
	if ((f & _DFLOW) && (f & _IMM)) {
		if (tA && (f & _rA)) dev_decode_regs(ia, rwA, tA, tU8);
		if (tB && (f & _rB)) dev_decode_regs(ia, rwB, tB, tU8);
	}
	
	// lookup symbolic names of memory references from "what" table above
	// includes device registers (distinguishes reads from writes), labeled ram & rom locations
	if (w) {
		if (((w->type == W_RAM) || ((w->type == W_READ) && !store) || ((w->type == W_WRITE) && store)) &&
			((!w->addr2 && w->addr == tAddr) || (tAddr >= w->addr && tAddr <= w->addr2))) {
			PF("[%s %c] %s ", space[w->type], (f & _R)? 'r':'w', w->name);
			dev_decode_regs(0, w->type == W_READ, tAddr, tU8);
		}

		if (w->type == W_END) {
			if ((tAddr & ROM_MASK) == ROM_START) PF("[rom] "); else
			if (tAddr & RAM_MASK) PF("[ram %c] ", (f & _R)? 'r':'w'); else
			PF("[dev] ADD TO LIST -------------------- ");
		}
	}

	// annotate subroutine & interrupt blocks, add labels
	if (f & _RTN) PF("\n-------------------------------------\n"); else
	if (f & _CALL) PF("\n\n"); else
		{
			for (i=0; w=&what[i], w->type != W_END; i++) {
				if ((w->type == W_COMMENT) && w->addr == rPC) {
					PF("// %s\n", w->name);
					break;
				}
			}
			if (w->type == W_END) PF("\n");
		}
}

void trace_iSnap(int _iSnap)
{
	iSnap = _iSnap;
	
	if (iSnap == 1) {
		trace_dump(); printf("---- snap ----------------\n");
	} else {
		trace_clear();
	}
}

void trace_iDump(int _iDump)
{
	iDump = _iDump;
	
	if (iDump == 1) {
		trace_dump(); printf("---- dump ----------------\n");
	} else {
		trace_clear();
	}
}

#endif
