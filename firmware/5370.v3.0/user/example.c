// an example of a measurement extension routine
//
// this code performs a number of TI measurements using the same technique as
// found in the disassembled HPIB binary transfer mode firmware

#include "boot.h"
#include "front_panel.h"
#include "5370_regs.h"

#include <string.h>
#include <stdio.h>

#define WREG_O3_RST		(O3_RST_TEST | O3_LOLRST | O3_CLR_EVT_OVFL | O3_HN3RST)
#define WREG_O2_ENA		(O2_FLAG | O2_LGATE | O2_LARMCT2 | O2_HRWEN_LRST)
#define WREG_O2_ARM		(O2_FLAG | O2_LGATE | O2_LARMCT2 | O2_HRWEN_LRST | O2_HMNRM)
#define WREG_O2_IDLE	(O2_FLAG | O2_LGATE | O2_LARMCT2)

void meas_extend_example(u1_t key)
{
	int i;
	u4_t n0st, n0st2, n1n2h, n1n2l, n0h, n0l;
	s4_t n1n2, n0;
	double ti;

	printf("measurement extension example called\n");
	dsp_7seg_str(2, "extend", TRUE);

	if (key & KEY(LCL_RMT)) {
		printf("LCL/RMT\n");
	}

#define N_TI	100000
	printf("taking %d TI measurements\n", N_TI);
	
	for (i=0; i<N_TI; i++) {
		bus_write(WREG_O3, WREG_O3_RST);
		bus_write(WREG_O2, WREG_O2_ENA);
		bus_write(WREG_O2, WREG_O2_ARM);
		
		// wait for end-of-measurement
		do {
			n0st = bus_read(RREG_N0ST);
		} while (n0st & N0ST_EOM);

		bus_write(WREG_O2, WREG_O2_IDLE);

		n0st = bus_read(RREG_N0ST);
		n1n2h = bus_read(RREG_N1N2H);
		n1n2l = bus_read(RREG_N1N2L);
		n1n2 = ((n0st & N0ST_N1N2) << 16) | (n1n2h << 8) | n1n2l;

		if (n0st & N0ST_PLL_OOL) printf("PLL UNLOCKED\n");
		if (n0st & N0ST_N0_OVFL) printf("N0 OVFL\n");

		// convert from 18-bit 2s complement
		if (n1n2 >= 0x20000) {
			n1n2 = n1n2 - 0x40000;
		}
		
		n0h = bus_read(RREG_N0H);
		n0l = bus_read(RREG_N0L);
		n0 = (n0h << 8) | n0l;

		// convert from 16-bit sign-magnitude
		if ((n0st & N0ST_N0_POS) == 0) {
			n0 = -n0;
		}

		//printf("n0st 0x%2x %d n1n2 %d %d n0 %d %d\n", n0st, n0st, n1n2h, n1n2l, n0h, n0l);
		ti = (((double) n1n2 / 256.0) + (double) n0) * 5.0e-9;
		bool rng = ((ti < 99.0e-9) || (ti > 99.9e+9))? TRUE:FALSE;
		if (rng) printf("out of range: 0x%02x\n", n0st & N0ST_MASK);

		if (i<10 || rng) {
			if (ti < 1.0e-8) {
				printf("%1.2f ns\n", ti * 1.0e8);
			} else {
				printf("%2.2f ns\n", ti * 1.0e9);
			}
		}
	}
	
	wait_key_release();

	printf("measurement extension example returning\n");
	dsp_7seg_str(0, "", TRUE);
}

void meas_extend_example_init()
{
	// Call us if MEAN and MIN pressed simultaneously or MEAN and LCL/RMT.
	// See the comment in the register_key_callback() routine for details.
	
	register_key_callback(KEY(MEAN) | KEY(MIN), meas_extend_example);
	register_key_callback(KEY(LCL_RMT) | KEY(MEAN), meas_extend_example);
}
