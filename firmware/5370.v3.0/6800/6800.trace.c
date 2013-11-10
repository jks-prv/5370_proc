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

char *deco[N_OP] = {
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

// most number of opcodes seen after instrument use, including (simple) HPIB, = 136
// 6800 spec says 197 possible, so we've not interpreted (197 - 136) = 61 of them
// output from the '@' command:

//opcode coverage 136/197 61
//not-interp: sba bhi bls tsx ins des txs wai asr-b neg-x@ com-x@ asl-x@ neg-## com-## ror-## asr-## asl-## rol-## sbc-a#
//bit-a@ eor-a@ lds-@ sts-@ sub-ax@ sbc-ax@ and-ax@ bit-ax@ eor-ax@ ora-ax@ cpx-x@ lds-x@ sts-x@ sbc-a## and-a## bit-a## eor-a## adc-a## ora-a## cpx-## lds-## sts-##
//sub-b# sbc-b# cmp-b@ bit-b@ ora-b@ sub-bx@ and-bx@ bit-bx@ eor-bx@ adc-bx@ add-bx@ stx-x@ sub-b## cmp-b## sbc-b## and-b## bit-b## adc-b## ora-b## add-b## 
//61 remaining


#define	NTRB	1024
static char trbuf[NTRB][128];
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
}

static char *last_s;

char *tr_s()
{
	// if the last use didn't end with a newline allow the string to be appended to
	if (last_s && (*(last_s + strlen(last_s) - 1) != '\n')) {
		last_s += strlen(last_s);
	} else {
		last_s = &trbuf[trbi++][0];
		if (trbi > NTRB) trbi=0;
	}
	
	return last_s;
}

void tr_printf(int ignore)
{
	if (!iTrace || iSnap || iDump) printf("%s", last_s);
}

typedef enum { W_RAM, W_READ, W_WRITE, W_LABEL, W_RANGE, W_END } what_e;

typedef struct {
	u2_t addr, addr2;
	what_e type;
	char *name;
} what_t;

#define	RAM(a, n)			a, 0, W_RAM, "ram: " # n
#define	READ(a)				a, 0, W_READ, # a
#define	WRITE(a)			a, 0, W_WRITE, # a
#define	RANGE_R(a, a2, n)	a, a2, W_READ, n
#define	RANGE_W(a, a2, n)	a, a2, W_WRITE, n
#define LABEL(a, n)			a, 0, W_LABEL, "label: " # n

static what_t what[] = {

	// display
	READ(RREG_KEY_SCAN),
	RANGE_W(ADDR_LEDS(12), ADDR_LEDS(0), "LEDs"),	// remember: reversed LED addressing
	RANGE_W(ADDR_7SEG(0), ADDR_7SEG(15), "7-seg display"),

	// arming
	READ(RREG_LDACSR),
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
	
	// ROM
	LABEL(0x76f6, "B = @b1 * 10"),
	LABEL(0x606e, "delay loop"),
	
	0, 0, W_END, ""
};

void trace(u2_t pc, u1_t irq, u1_t a, u1_t b, u2_t x, u2_t sp, u1_t C, u1_t VNZ)
{
	int i, store, imm;
	u1_t opcode, b2, b3, tU8;
	u2_t tAddr, tU16;
	u1_t *rom = &rom_image[0] - ROM_START;
	what_t *w;
	
	opcode = rom[pc];
	b2 = rom[pc+1];
	b3 = rom[pc+2];
	
	// skip delay loops, etc.
	if ((pc >= 0x6064) && (pc <= 0x606c)) return;

	PF("%04x%c A=%02x B=%02x X=%04x SP=%04x %c%c%c%c %02x %02x %02x %s ",
		pc, irq? '*':' ', a, b, x, sp,
		getC()?'C':'c', getV()?'V':'v', getN()?'N':'n', getZ()?'Z':'z',
		opcode, b2, b3, deco[opcode]);
	
	pc++;
	imm = 1;

#define	CMPX_WIM	0x8c
#define	LDS_WIM		0x8e
#define LDX_WIM		0xce
	
	// imm 16-bit (_16_WIM)
	if ((opcode == CMPX_WIM) || (opcode == LDS_WIM) || (opcode == LDX_WIM)) {
			tU16 = (b2 << 8) | b3;
			PF("#=%04x ", tU16);
	} else {
	
		switch (opcode >> 4) {
		
		// imm 8-bit
		case 0x8:
		case 0xc:
			tU8 = b2;
			PF("#=%02x ", tU8);
			break;
	
		// dir
		case 0x9:
		case 0xd:
			tAddr = (u2_t) b2;
			PF("@=%02x ", tAddr);
			imm = 0;
			break;
		
		// ext
		case 0x7:
		case 0xb:
		case 0xf:
			tAddr = (b2 << 8) | b3;
			PF("@=%04x ", tAddr);
			imm = 0;
			break;
		
		// idx
		case 0x6:
		case 0xa:
		case 0xe:
			tAddr = x + (u2_t) b2;
			PF("@=%04x(rX+%02x) ", tAddr, (u2_t) b2);
			imm = 0;
			break;
			
		}
	}
	
	// lookup symbolic names of memory addresses from "what" table above
	// includes device registers (distinguishes reads from writes), labeled ram & rom locations
	if (!imm) {
		u1_t oph, opl;
		opl = opcode & 0xf;
		oph = opcode >> 8;
		
		// decode which opcodes do stores
		if (((opl == 0x7) && ("1110111000000000"[oph]=='1')) ||
			((opl == 0xf) && ("1110111011000000"[oph]=='1'))) store=1; else store=0;

		for (i=0; w=&what[i], w->type != W_END; i++) {
			if (((w->type == W_RAM) || ((w->type == W_READ) && !store) || ((w->type == W_WRITE) && store)) &&
				((!w->addr2 && w->addr == tAddr) || (tAddr >= w->addr && tAddr <= w->addr2))) {
				PF("%s ", w->name);
				break;
			}
		}

		if (w->type == W_END) {
			if (tAddr & RAM_MASK) PF("[ram] "); else
			if ((tAddr & ROM_MASK) == ROM_START) PF("[rom] "); else
			PF("[dev] ADD TO LIST -------------------- ");
		}
	}

#define	SWI		0x3f
#define	RTI		0x3b
#define	RTS		0x39
#define	JSR		0xbd
#define	JSR_IDX	0xad
#define	BSR		0x8d

	// annotate subroutine & interrupt blocks, add labels
	if (opcode == SWI || opcode == RTI) PF("\n-------------------------------------\n"); else
	if (opcode == RTS || opcode == JSR || opcode == JSR_IDX || opcode == BSR) PF("\n\n"); else
		{
			for (i=0; w=&what[i], w->type != W_END; i++) {
				if (w->type == W_LABEL && w->addr == (pc-1)) {
					PF("%s\n", w->name);
					break;
				}
			}
			if (w->type == W_END) PF("\n");
		}
}

#endif
