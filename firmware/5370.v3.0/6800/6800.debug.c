#if defined(DEBUG) || defined(INSN_TRACE)

	if (*cp == '.') {
		printf("rPC 0x%04x\n", rPC);
	} else

	// start history recording of instruction trace to be dumped by "snap" (or dump) trigger later on
	if (*cp == 't') {
		iTrace ^= 1;
		trace_clear();
		printf("trace %d\n", iTrace);
	} else

	// dump tracing history buffer, if enabled, and trace a few instructions after
	if (*cp == 'i') {
		trace_iSnap(1);
	} else

	// trace all instructions from this point forward
	if (*cp == 'x') {
		trace_iDump(iDump^1);
	} else

	#define APPEND_ADDR(per_line, addr) \
			if ((i % (per_line)) == ((per_line)-1)) printf("  0x%x\n", i - ((per_line)-1) + (addr));

#ifdef ROM_RAM_COVER
	if (*cp == 'c') {
		int i, j;
		
		printf("ROM/RAM access coverage: T=touched, .=not\n\n");
		
		j = 0;
		for (i = 0; i < ROM_SIZE; i++) {
			if (rom_coverage[i] == 0xee) {
				printf("T");
				j++;
			} else {
				printf(".");
			}
			APPEND_ADDR(128, ROM_START);
		}
		printf("ROM coverage %d/%d\n", j, ROM_SIZE);

		j = 0;
		for (i = 0; i < RAM_SIZE; i++) {
			if (ram_coverage[i] == 0xee) {
				printf("T");
				j++;
			} else {
				printf(".");
			}
			APPEND_ADDR(32, RAM_START);
		}
		printf("RAM coverage %d/%d\n", j, RAM_SIZE);
	} else
#endif

#ifdef DEBUG
	if (*cp == '*') {
		irq_trace = 1;
	} else

	// slow down interpreter by some amount to debug timing effects
	// (enable MEAS_IPS define to see how fast it's running)
	if (sscanf(cp, "s %d", &ispeed) == 1) {
		printf("ispeed %d\n", ispeed);
	} else

	if (*cp == '!') {
		int i;
		printf("raw opcode interp counts:\n");
		for (i = 0; i < N_OP; i++) {
			if (op_coverage[i] != 0)
				printf("%s=%d ", deco[i], op_coverage[i]);
		}
		printf("\n");
	} else

	if (*cp == '#') {
		int i, j, k;
		printf("sorted opcode interp counts:\n");
		
		// I hope Don Knuth doesn't catch me sorting this way. Sorry, I'm in a hurry.
		for (i = 0; i < N_OP; i++) {
			sorted[i].count = 0;
		}
		
		for (i = 0; i < N_OP; i++) {
			k = -1;
			for (j = 0; j < N_OP; j++) {
				if (op_coverage[j] > sorted[i].count) {
					sorted[i].count = op_coverage[j];
					sorted[i].idx = j;
					k = j;
				}
			}
			if (k != -1)
				op_coverage[k] = 0;
		}
		
		for (i = 0; i < N_OP; i++) {
			if (sorted[i].count != 0) {
				printf("%s=%d ", deco[sorted[i].idx], sorted[i].count);
			}
		}
		printf("\n");
	} else

	if (*cp == '@') {
		int i, j;
		for (i = j = 0; i < N_OP; i++) {
			if (op_coverage[i]) j++;
		}
		printf("opcode coverage %d/%d %d\n", j, N_LEGIT_OP, N_LEGIT_OP - j);
		printf("not interp: ");
		for (i = j = 0; i < N_OP; i++) {
			if ((op_coverage[i] == 0) && (deco[i][0] != 'x')) {
				printf("%s ", deco[i]);
				j++;
			}
		}
		printf("\n%d remaining\n", j);
	} else
#endif

#ifdef RECORD_IO_HIST
	if (*cp == 'o') {
		int i;
		for (i=0; i<IO_HIST; i++) {
			if (io_hist[i]) printf("%4d: %d\n", i, io_hist[i]);
		}
	} else
#endif

#endif
	{
		printf("unknown command: %s", cp);
	}
