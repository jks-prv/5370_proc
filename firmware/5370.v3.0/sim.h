#ifndef _SIM_H_
#define _SIM_H_

#include "config.h"

// how many times through the simulator loop before input poll check
// -1 because used as a count compare mask, e.g.
// if ((iCount & SIM_POLL_INPUT_COUNT) == 0) ...
#define SIM_POLL_INPUT_COUNT	(1024-1)

// max number of times through simulator loop before network poll check
#define SIM_POLL_NET_COUNT		(1024-1)

extern u1_t sim_key, sim_key_intr;

void sim_main();
void sim_processor();
u4_t sim_insn_count();
char *sim_input();

#define	PF(...)		tr_printf(sprintf(tr_s(), __VA_ARGS__))
#define	PFC(c, ...)	if (c) tr_printf(sprintf(tr_s(), __VA_ARGS__))

void trace(u2_t pc, u1_t irq, u1_t a, u1_t b, u2_t x, u2_t sp, u1_t C, u1_t VNZ);
void trace_dump();
void trace_clear();
char *tr_s();
void tr_printf(int ignore);

extern bool iDump;
extern int iSnap, iTrace;

#endif
