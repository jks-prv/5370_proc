#include "boot.h"
#include "front_panel.h"
#include "5370_regs.h"
#include "hpib.h"
#include "chip.h"

#include <string.h>
#include <stdio.h>

#ifdef DEBUG

void find_bug()
{
	int i, ok, bad;
	u4_t n0st, n0st2, n1n2h, n1n2l, n0h, n0l;
	s4_t n1n2, n0;
	double ti;

	printf("looking for bug..\n");
	i=1; ok=bad=0;
	
	while (i++) {
		bus_write(WREG_O3, WREG_O3_RST);
		bus_write(WREG_O2, WREG_O2_ENA);
		bus_write(WREG_O2, WREG_O2_ARM);
		
		// wait for end-of-measurement
		do {
			n0st = bus_read(RREG_N0ST);
		} while (isInactive(N0ST_EOM, n0st));

		bus_write(WREG_O2, WREG_O2_IDLE);

		n0st = bus_read(RREG_N0ST);
		n1n2h = bus_read(RREG_N1N2H);
		n1n2l = bus_read(RREG_N1N2L);
		n1n2 = ((n0st & N0ST_N1N2) << 16) | (n1n2h << 8) | n1n2l;

		if (isActive(N0ST_PLL_OOL, n0st)) printf("PLL UNLOCKED\n");
		if (isActive(N0ST_N0_OVFL, n0st)) printf("N0 OVFL\n");

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
		//printf("N0 %ld N1N2 %ld 5ns %1.2f\n", n0, n1n2, ((double) n1n2 / 256.0) + (double) n0);
		ti = (((double) n1n2 / 256.0) + (double) n0) * 5.0e-9;
		bool rng = ((ti < 98.0e-9) || (ti > 101.1e+9))? TRUE:FALSE;
		if (((i&0xffff)==0)) { printf("."); fflush(stdout); }

		if (rng) {
			check_pmux();
			if (ok) {
				printf("ok %d BAD: ", ok); ok=0;
				if (ti < 1.0e-8) {
					printf("%1.2f ns\n", ti * 1.0e8);
				} else {
					printf("%2.2f ns\n", ti * 1.0e9);
				}
			}
			bad++;
		} else {
			if (bad) { printf("bad %d\n", bad); bad=0; }
			ok++;
		}
	}
}

#endif
