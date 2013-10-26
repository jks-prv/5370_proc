// 6800 microprocessor interpreter
//
// some of the syntactic complexity remaining here is leftover from when this code ran on a far less-capable
// microcontroller (Atmel sam7x) and used some fancy dynamic inline code generation to speed things up

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

// these are set in the makefile:
// DEBUG
// INSN_TRACE

#define INT_NMI  1
#define INT_FIRQ 2
#define INT_IRQ  4

// redefined from 6800 standard 0xfffx to speed memory access routines via easier address decoding
#define	VEC_RESET	0x7ffe
#define	VEC_NMI		0x7ffc
#define	VEC_IRQ		0x7ff8
#define	VEC_SWI		0x7ffa

u1_t rom_image[ROM_SIZE] = {
	#include ROM_CODE_H
};

u1_t ram_image[RAM_SIZE];

// for some reason defining readByte in the block-macro style fails at runtime!
#define readByte(addr) \
	(((addr & ROM_MASK) != 0)? (rom[addr]) : \
	(((addr & RAM_MASK) != 0)? (ram[addr]) : \
	readDev(addr)))

// readByteNoROM helps optimize the following case: INSN(0x6c, inc, _08_IDX(0x0000); RMW_8(tU8 = inc(tU8)) ); et al.
// i.e. _08_IDX means tAddr = rX + rom_const, which could point anywhere, but RMW means it can't be ROM due to the write.
// So readByteNoROM is used in the implementation of RMW_8.
// The -O3 optimizer can't catch this because tAddr isn't constant in the _08_IDX case.
//
// Normally -O3 can optimize readByte because tAddr is constant for the inline ROM code.

#define readByteNoROM(addr) \
	(((addr & RAM_MASK) != 0)? (ram[addr]) : \
	readDev(addr))

#define readWord(pAddr) \
({ \
	u2_t addr = pAddr; \
	u1_t hi, lo; \
	\
	hi = readByte(addr); \
	addr++; \
	lo = readByte(addr); \
	(hi << 8) | lo; \
})

// assumes push/pull always goes to RAM and never device space
#define pullByte(addr) \
	ram[addr]

#define pullWord(pAddr) \
({ \
	u2_t addr = pAddr; \
	u1_t hi, lo; \
	\
	hi = pullByte(addr); \
	addr++; \
	lo = pullByte(addr); \
	(hi << 8) | lo; \
})

#define writeByte(addr, data) \
	if (((addr) & RAM_MASK) != 0) { \
		ram[addr] = data; \
	} else { \
		writeDev(addr, data); \
	}

#define writeWord(pAddr, p16) \
({ \
	u2_t addr = pAddr; \
	u2_t d16 = p16; \
	u1_t d8; \
	\
	d8 = d16 >> 8; \
	writeByte(addr, d8); \
	addr++; \
	d8 = d16 & 0xff; \
	writeByte(addr, d8); \
})

#define pushByte(addr, data) \
	ram[addr] = data;

#define pushWord(pAddr, p16) \
({ \
	u2_t addr = pAddr; \
	u2_t d16 = p16; \
	u1_t d8; \
	\
	d8 = d16 >> 8; \
	pushByte(addr, d8); \
	addr++; \
	d8 = d16 & 0xff; \
	pushByte(addr, d8); \
})


// used to measure histogram of instruction counts between i/o cycles
#ifdef RECORD_IO_HIST

	#define IO_HIST 1024
	static u4_t io_hist[IO_HIST], io_hist_last;
	
	void io_hist_upd(iCount)
	{
		u4_t t = iCount - io_hist_last;
		if (t > IO_HIST) t = 0;
		io_hist[t]++;
		io_hist_last = iCount;
	}
	
	#undef readByte
	
	#define readByte(addr) \
		(((addr & ROM_MASK) != 0)? (rom[addr]) : \
		(((addr & RAM_MASK) != 0)? (ram[addr]) : \
		(io_hist_upd(), readDev(addr))))
	
	#undef readByteNoROM
	#define readByteNoROM(addr) readByte(addr)
	
	#undef writeByte
	
	#define writeByte(addr, data) \
		if ((addr & RAM_MASK) != 0) { \
			ram[addr] = data; \
		} else { \
			io_hist_upd(iCount); \
			writeDev(addr, data); \
		}

#endif


#if defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)

	#undef readByte
	
	#define readByte(addr) \
		(((addr & ROM_MASK) != 0)? (rom[addr]) : \
		(((addr & RAM_MASK) != 0)? (ram[addr]) : \
		readDev(addr, iCount, rPC, 0, getI())))
	
	#undef readByteNoROM
	#define readByteNoROM(addr) readByte(addr)
	
	#undef writeByte
	
	#define writeByte(addr, data) \
		if ((addr & RAM_MASK) != 0) { \
			ram[addr] = data; \
		} else { \
			writeDev(addr, data, iCount, rPC, 0, getI()); \
		}

#endif


//#define ROM_RAM_COVER
#ifdef ROM_RAM_COVER
	u1_t rom_coverage[ROM_SIZE];
	u1_t ram_coverage[RAM_SIZE];
	
	int cover_sampling = 0;
	
	#undef readByte
	
	u1_t readByte(u2_t addr)
	{
		if ((addr & ROM_MASK) != 0) {
			if (cover_sampling) rom_coverage[addr-ROM_START] = 0xee;
			return rom_image[addr-ROM_START];
		} else
		if ((addr & RAM_MASK) != 0) {
			if (cover_sampling) ram_coverage[addr-RAM_START] = 0xee;
			return ram_image[addr-RAM_START];
		}
	
		return readDev(addr);
	}
	
	#undef readByteNoROM
	#define readByteNoROM(addr) readByte(addr)

#endif


#if defined(DEBUG) || defined(INSN_TRACE)

#define N_OP		256
#define N_LEGIT_OP	197

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

#endif

#include "6800.cc.h"
#include "6800.core.h"


#if defined(DEBUG) || defined(INSN_TRACE)

int itrace = 0;
int itr = 0;

void trace(u2_t pc, u1_t irq, u1_t a, u1_t b, u2_t x, u2_t sp)
{
	u1_t opcode, b2, b3, tU8;
	u2_t tAddr, tU16;
	u1_t *rom = &rom_image[0] - ROM_START;
	
	opcode = rom[pc];
	b2 = rom[pc+1];
	b3 = rom[pc+2];
	printf("%04x%c A=%02x B=%02x X=%04x SP=%04x %02x %02x %02x %s ",
		pc, irq? '*':' ', a, b, x, sp, opcode, b2, b3, deco[opcode]);
	//printf("."); fflush(stdout);
	
	pc++;
	
	// imm 16-bit (_16_WIM)
	if ((opcode == 0x8c) || (opcode == 0x8e) || (opcode == 0xce)) {
			tU16 = rom[pc];
			printf("#=%04x ", tU16);
	} else {
	
		switch (opcode >> 4) {
		
		// imm
		case 0x8:
		case 0xc:
			tU8 = rom[pc];
			printf("#=%02x ", tU8);
			break;
	
		// dir
		case 0x9:
		case 0xd:
			tAddr = (u2_t) rom[pc];
			printf("@=%02x ", tAddr);
			break;
		
		// ext
		case 0x7:
		case 0xb:
		case 0xf:
			tAddr = (rom[pc] << 8) | rom[pc+1];
			printf("@=%04x ", tAddr);
			break;
		
		// idx
		case 0x6:
		case 0xa:
		case 0xe:
			tAddr = x + (u2_t) rom[pc];
			printf("@=%04x(rX+%02x) ", tAddr, (u2_t) rom[pc]);
			break;
			
		}
	}
	
	printf("\n");
}

#endif

static int IRQs = 0;

#ifdef DEBUG
	bool iDump;
	int iSnap = 0;
	int irq_trace = 0;
	
	u4_t op_coverage[N_OP];
	
	struct {
		u4_t count;
		u1_t idx;
	} sorted[N_OP];
#endif

#if defined(RECORD_IO_HIST) || defined(DEBUG) || defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG) || defined(MEAS_IPS) || defined(MEAS_IP_HPIB_MEAS)
	#define ICOUNT
	#define INT_ICOUNT
	#define iCount (ciCount + iiCount)
	#define INLINE_ICOUNT()	ciCount++
	#define INTERP_ICOUNT() iiCount++
#else
	#define iCount 0
	#define INLINE_ICOUNT()
	#define INTERP_ICOUNT()
#endif

#if defined(ICOUNT) && (defined(MEAS_IP_HPIB_MEAS) || defined(HPIB_SIM_DEBUG))
	#undef INT_ICOUNT
	#define EXT_ICOUNT
#endif

#ifdef EXT_ICOUNT
	static u4_t ciCount, iiCount;
	
	u4_t sim_insn_count()
	{
		return ciCount + iiCount;
	}
#endif

#define PEND_PRINT	8

void sim_processor()
{
	u1_t rA, rB;
	u2_t rX, rSP, rPC;

	declareCC();

	u2_t tAddr = 0;
	u1_t tU8 = 0;
	u2_t tU16 = 0;

	// biased with e.g. ROM_START so that rom[addr] can be indexed with actual runtime address
	u1_t *rom = &rom_image[0] - ROM_START;
	u1_t *ram = &ram_image[0] - RAM_START;
	
	s2_t tS16 = 0;

	u1_t opcode;

#ifdef INT_ICOUNT
	u4_t ciCount, iiCount;
#endif

#if defined(DEBUG) || defined(DBG_INTRS)
	u4_t iPendCt = 0;
	u4_t iPendPrint = PEND_PRINT;
#endif

#ifdef DEBUG
	u1_t inh;
	u4_t inIRQ = 0;
#endif

#ifdef DBG_INTRS
	u4_t i_level = 0, i_latched = 0;
#endif

#ifdef FASTER_INTERRUPTS
	extern volatile bool got_irq;
#endif

D_STMT(reset:)
	D_STMT(inh = 0);
	
#ifdef INT_ICOUNT
	ciCount = iiCount = 0;
#endif

#ifdef ROM_RAM_COVER
	{
		int i;
		
		for (i = 0; i < ROM_SIZE; i++) {
			rom_coverage[i] = 0;
		}
		for (i = 0; i < RAM_SIZE; i++) {
			ram_coverage[i] = 0;
		}
	}
#endif
	
	rA = rB = 0;
	rX = rSP = 0;
	
#if defined(HPIB_RECORD) || defined(HPIB_SIM_DEBUG)
	rPC = IRQ = 0;		// avoid compiler uninitialized warning
#endif

	rPC = readWord(VEC_RESET);
	D_PRF(("DEBUG MODE ON -- START @ %04x\n", rPC));

	setCC(0);
	setI();	// no IRQ in initially

next_insn:
branch_taken:

	if ((iCount & SIM_POLL_NET_COUNT) == 0) net_poll();

	char *cp;
	if (((iCount & SIM_POLL_INPUT_COUNT) == 0) && ((cp = sim_input()) != 0)) {

#include "6800.debug.c"

	}

//#define SHOW_INTERP
#ifdef SHOW_INTERP
	// toggle the "test pin" to show when the interp is running
	// helps us see on the logic analyzer when we're in the network stack
	if (iCount & 1) {
		TEST2_SET();
	} else {
		TEST2_CLR();
	}
#endif

#ifdef DEBUG
	if ((rPC < ROM_START) || (rPC >= (ROM_START+ROM_SIZE))) {
   		printf("\nrPC range! 0x%x 0x%x-0x%x\n", rPC, ROM_START, ROM_START+ROM_SIZE);
   		panic("");
	}
#endif

checkpending:

#if	defined(DEBUG) || defined(INSN_TRACE)
	itr = 0;
#endif
	
#ifdef FASTER_INTERRUPTS
	if (got_irq || hpib_causeIRQ) {
		got_irq = FALSE;
#endif

	if (CHECK_NMI()) {	// FIXME interrupt routine could set IRQs directly with proper locking
		#ifdef DBG_INTRS
			printf("*NMI*\n");
		#endif
		IRQs |= INT_NMI;
	} else {
		IRQs &= ~INT_NMI;
	}

#if 0
	i_level = 0;
	char *cp;
	if ((cp = sim_input())) {
		if (*cp == 'l' && cp[1] == '1') i_level = 1;
		if (*cp == 'l' && cp[1] == '2') i_latched = 1;
	}
	if (CHECK_IRQ() || sim_key || i_level || i_latched) {
#else
	if (CHECK_IRQ() || sim_key) {
#endif
		#ifdef DBG_INTRS
			if (iPendCt == 0) printf("*IRQ* msk? %s " , getI()? "YES":"NO");
		#endif
		IRQs |= INT_IRQ;
	} else {
		IRQs &= ~INT_IRQ;
	}

#ifdef HPIB_SIM
	if (hpib_causeIRQ) {
		#ifdef DBG_INTRS
			printf("*HIRQ*\n");
		#endif
		#ifdef HPIB_SIM_DEBUG
			if (hps) printf("----HPIB IRQ\n");
		#endif
		IRQs |= INT_IRQ;
	}
#endif

	if (IRQs) {

		if (IRQs & INT_NMI) {	// NMI is highest priority
			pushW(rPC);
			pushW(rX);
			pushB(rA);
			pushB(rB);
			pushB(getCC());
			setI();		// mask interrupts during service routine
			rPC = readWord(VEC_NMI);
			IRQs &= ~INT_NMI;
#ifdef INSN_TRACE
			if (itr) {
				printf("-------- NMI\n");
			}
#endif
#ifdef DEBUG
			//printf("NMI@0x%x ", rPC); fflush(stdout);
#endif
#ifdef DBG_INTRS
			iPendCt = 0;
			iPendPrint = PEND_PRINT;
#endif
			goto doexecute;
		}

		if ((IRQs & INT_IRQ) && (getI() == 0)) {	/* IRQ is lowest priority */
			pushW(rPC);
			pushW(rX);
			pushB(rA);
			pushB(rB);
			pushB(getCC());
			setI(); /* Mask interrupts during service routine */
			rPC = readWord(VEC_IRQ);
			IRQs &= ~INT_IRQ; /* clear this bit */

#ifdef HPIB_SIM
			hpib_causeIRQ = FALSE;
#endif

#ifdef INSN_TRACE
			if (itr) {
				printf("-------- IRQ\n");
			}
#endif
#ifdef DEBUG
			//printf("IRQ@0x%x ", rPC); fflush(stdout);
	#ifdef HPIB_RECORD
			writeDev(0, 0, iCount, rPC, 1, getI());		// record the interrupt event
	#endif
#endif
#ifdef DBG_INTRS
			if (iPendCt)
				printf("[TAKE iPEND=%d --->]\n", iPendCt);
			else
				printf("TAKE--->\n");
			iPendCt = 0;
			iPendPrint = PEND_PRINT;
			i_latched = 0;
#endif
			goto doexecute;
		}
	}

#ifdef DBG_INTRS
	if (IRQs & INT_IRQ) {
		iPendCt++;
		if (iPendCt == iPendPrint) {
			//printf("iPEND-%d ", iPendCt); fflush(stdout);
			printf("iPEND-%d ", iPendCt);
			iPendPrint <<= 2;
		}
	} else {
		if (iPendCt) {
			printf("[GONE iPEND=%d msk? %s]\n", iPendCt, getI()? "YES":"NO");
			iPendCt = 0;
			iPendPrint = PEND_PRINT;
		}
	}
#endif

doexecute: ;

#ifdef FASTER_INTERRUPTS
	}
#endif

#ifdef ROM_RAM_COVER
	if(cover_sampling) rom_coverage[rPC-ROM_START] = 0xee;
#endif

#ifdef DEBUG
	
	inIRQ = !getI();

	opcode = rom[rPC];
	op_coverage[opcode]++;
	
	if (irq_trace && inIRQ) {
		itr |= 1;
	}
	
	if (iSnap && (iSnap++ < 16)) {
		itr |= 1;
	} else {
		iSnap = 0;
	}

	if (iDump) {
		itr |= 1;
	}

	#ifdef INSN_TRACE
		if (itrace) {
			itr |= 1;
		}
	#endif
	
	// don't print trace when in this particular delay loop
	if ((itr == 0) || ((rPC >= 0x6064) && (rPC <= 0x606f))) inh = 1; else inh = 0;

#endif

#ifdef MEAS_IPS
	{
		u4_t now;
		static u4_t last, last_ciCount, last_iiCount, last_iCount;

		now = sys_now();
		if (time_diff(now, last) >= 1000) {
			u4_t cic, iic;
			char *cicu=" ", *iicu=" ";
			u4_t icnt = ciCount + iiCount;
			if ((cic = ciCount-last_ciCount) > 1024) {
				cic /= 1024;
				cicu = "K";
			}
			if ((iic = iiCount-last_iiCount) > 1024) {
				iic /= 1024;
				iicu = "K";
			}
			printf("%4d%s + %4d%s = %4dK i/sec\n", cic, cicu, iic, iicu, (icnt-last_iCount)/1024);
			last = now;
			last_ciCount = ciCount;
			last_iiCount = iiCount;
			last_iCount = icnt;
		}
	}
#endif
	
#ifdef ROM_RAM_COVER
	
	// wait until after self-test checksum routine that touches all locations of ROM and RAM
	if (iCount > 50000) {
		cover_sampling = 1;
	}
#endif

	// interpreter

	#define _4CALL(dummy_dest)		goto branch_taken;
	#define BRANCH_ALWAYS()			goto branch_taken;
	#define BRANCH_TAKEN()			goto branch_taken;
	#define _2RESTORE_PC(pc)		
	#define CAPTURE_PC()		
	
	#define INSN(opc, opn, x) \
			case opc: \
				asm("# ---- " #opc " " #opn " ----------------");	/* asm appears as a comment in the compiled code */ \
				{ x; }; \
			break;

#ifdef INSN_TRACE
	if (itr) trace(rPC, getI(), rA, rB, rX, rSP);
#endif

	opcode = rom[rPC];
	rPC++;
	INTERP_ICOUNT();

	switch (opcode) {

		#ifdef ROM_RAM_COVER
		
			#define _08_REL(rom_const)		tS16 = (s2_t) (s1_t) rom[rPC]; rPC++; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
			
			#define _08_IMM(rom_const)		tU8 = rom[rPC]; rPC++; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
			
			#define _16_WIM(rom_const)		tU16 = (rom[rPC] << 8) | rom[rPC+1]; rPC += 2; \
											if (cover_sampling) rom_coverage[rPC-2-ROM_START] = 0xee; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
			
			#define _08_DIR(rom_const)		tAddr = (u2_t) rom[rPC]; rPC++; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
			
			#define _16_EXT(rom_const)		tAddr = (rom[rPC] << 8) | rom[rPC+1]; rPC += 2; \
											if (cover_sampling) rom_coverage[rPC-2-ROM_START] = 0xee; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
			
			#define _08_IDX(rom_const)		tAddr = rX + (u2_t) rom[rPC]; rPC++; \
											if (cover_sampling) rom_coverage[rPC-1-ROM_START] = 0xee;
		
		#else
		
			// definition of the various addressing modes
			#define _08_REL(rom_const)		tS16 = (s2_t) (s1_t) rom[rPC]; rPC++;
			#define _08_IMM(rom_const)		tU8 = rom[rPC]; rPC++;
			#define _16_WIM(rom_const)		tU16 = (rom[rPC] << 8) | rom[rPC+1]; rPC += 2;
			#define _08_DIR(rom_const)		tAddr = (u2_t) rom[rPC]; rPC++;
			#define _16_EXT(rom_const)		tAddr = (rom[rPC] << 8) | rom[rPC+1]; rPC += 2;
			#define _08_IDX(rom_const)		tAddr = rX + (u2_t) rom[rPC]; rPC++;
		
		#endif
		
		// code implementing the interpretation of the instruction set is inserted here
		#include "6800.insns.h"
	
		default:
			D_PRF(("opc 0x%x rPC 0x%x\n", tU8, rPC));
			panic("bad opcode");
			break;
	}

	if (!sys_reset) goto next_insn;
}
