//
// Code that runs on the PRU to count overflows of the N0/N3 hardware registers and
// perform 5370 bus cycles.
//

.origin 0
.entrypoint start

#include "pru_realtime.hp"

	// shared memory used to communicate with CPU
	// layout must match pru_realtime.h
	.struct	m
		.u32	cmd
		.u32	count
		.u32	watchdog
		
		.u32	p0
		.u32	p1
		.u32	p2
		.u32	p3

		.u32	n0_ovfl					// overflow counts
		.u32	n3_ovfl
		.u32	ovfl_none

		.u32	a_gen_0				// pre-scrambled address for general bus i/o
		.u32	a_gen_0c
		.u32	a_gen_1
		.u32	a_gen_1c
		.u32	a_gen_3
		.u32	a_gen_3c

		.u32	a_n0st_0				// pre-scrambled RREG_N0ST address
		.u32	a_n0st_0c
		.u32	a_n0st_1
		.u32	a_n0st_1c
		.u32	a_n0st_3
		.u32	a_n0st_3c

		.u32	a_n0_h_0
		.u32	a_n0_h_0c
		.u32	a_n0_h_1
		.u32	a_n0_h_1c
		.u32	a_n0_h_3
		.u32	a_n0_h_3c

		.u32	a_n1n2_h_0
		.u32	a_n1n2_h_0c
		.u32	a_n1n2_h_1
		.u32	a_n1n2_h_1c
		.u32	a_n1n2_h_3
		.u32	a_n1n2_h_3c

		.u32	a_o3_0					// pre-scrambled RREG_O3 address
		.u32	a_o3_0c
		.u32	a_o3_1
		.u32	a_o3_1c
		.u32	a_o3_3
		.u32	a_o3_3c

		.u32	d_n0_clr_ovfl_0			// pre-scrambled WREG_O3_N0_CLR_OVFL data
		.u32	d_n0_clr_ovfl_0c
		.u32	d_n0_clr_ovfl_1
		.u32	d_n0_clr_ovfl_1c
		.u32	d_n0_clr_ovfl_2
		.u32	d_n0_clr_ovfl_2c

		.u32	d_n3_clr_ovfl_0			// pre-scrambled WREG_O3_N3_CLR_OVFL data
		.u32	d_n3_clr_ovfl_0c
		.u32	d_n3_clr_ovfl_1
		.u32	d_n3_clr_ovfl_1c
		.u32	d_n3_clr_ovfl_2
		.u32	d_n3_clr_ovfl_2c
	.ends

	.struct	m2
		.u32	m2_offset
		
		.u32	d_gen_0				// pre/post-scrambled read/write data for general bus i/o
		.u32	d_gen_0c
		.u32	d_gen_1
		.u32	d_gen_1c
		.u32	d_gen_2
		.u32	d_gen_2c

		// rest are for PRU_TI_MEAS
		.u32	ad_rst_0
		.u32	ad_rst_0c
		.u32	ad_rst_1
		.u32	ad_rst_1c
		.u32	ad_rst_2
		.u32	ad_rst_2c
		.u32	ad_rst_3
		.u32	ad_rst_3c

		.u32	ad_ena_0
		.u32	ad_ena_0c
		.u32	ad_ena_1
		.u32	ad_ena_1c
		.u32	ad_ena_2
		.u32	ad_ena_2c
		.u32	ad_ena_3
		.u32	ad_ena_3c

		.u32	ad_arm_0
		.u32	ad_arm_0c
		.u32	ad_arm_1
		.u32	ad_arm_1c
		.u32	ad_arm_2
		.u32	ad_arm_2c
		.u32	ad_arm_3
		.u32	ad_arm_3c

		.u32	ad_idle_0
		.u32	ad_idle_0c
		.u32	ad_idle_1
		.u32	ad_idle_1c
		.u32	ad_idle_2
		.u32	ad_idle_2c
		.u32	ad_idle_3
		.u32	ad_idle_3c

		.u32	d_n0st_0
		.u32	d_n0st_1
		.u32	d_n0st_2

		.u32	d_n1n2_h_0
		.u32	d_n1n2_h_1
		.u32	d_n1n2_h_2

		.u32	d_n1n2_l_0
		.u32	d_n1n2_l_1
		.u32	d_n1n2_l_2

		.u32	d_n0_h_0
		.u32	d_n0_h_1
		.u32	d_n0_h_2

		.u32	d_n0_l_0
		.u32	d_n0_l_1
		.u32	d_n0_l_2
	.ends

	// register aliases
	.struct	regs
		.u32	m
		.u32	m2
		.u32	gpio0
		.u32	gpio1
		.u32	gpio2
		.u32	gpio3
		.u32	gpio_in
		.u32	gpio_out
		.u32	gpio_oe
		.u32	gpio_set
		.u32	gpio_clr
		.u32	timer
		.u32	timer_tclr
		.u32	timer_tcrr
		.u32	p0
		.u32	p1
		.u32	p2
		.u32	p3
		.u32	p4
		.u32	p5
		.u32	n0_ovfl
		.u32	n3_ovfl
		.u32	ovfl_none
		.u16	ra1
		.u16	ra2
		.u16	ra3
		.u16	ra4
		.u32	io31
	.ends
	
	.assign	regs, r6, r31, r			// r0-r5 are temps, avoid r30-r31

start:
    // enable OCP master ports so we can access gpio & timer
    lbco	r1, c4, 4, 4				// SYSCFG register (C4 in constant table)
    clr		r1, r1, 4					// clear standby_init bit
    sbco	r1, c4, 4, 4

	// address pointers (too big for immediate field in most cases)
	mov		r.m, PRU_COM_MEM
	mov		r.m2, PRU_COM_MEM2
	mov		r.gpio0, GPIO0_BASE
	mov		r.gpio1, GPIO1_BASE
	mov		r.gpio2, GPIO2_BASE
	mov		r.gpio3, GPIO3_BASE
	mov		r.gpio_in, _GPIO_IN
	mov		r.gpio_out, _GPIO_OUT
	mov		r.gpio_oe, _GPIO_OE
	mov		r.gpio_set, _GPIO_SET
	mov		r.gpio_clr, _GPIO_CLR
	mov		r.timer, TIMER4_BASE
	mov		r.timer_tclr, _TIMER_TCLR
	mov		r.timer_tcrr, _TIMER_TCRR

	mov		r1, 0
	eput	r1, r.m, m.watchdog
	jmp		cmd_clear

cmd_done:
	mov		r1, PRU_DONE
    eput	r1, r.m, m.cmd
    
cmd_get:
	eget	r1, r.m, m.cmd
	qbeq	cmd_ping, r1, PRU_PING
	qbeq	cmd_read, r1, PRU_READ
	qbeq	cmd_write, r1, PRU_WRITE
	qbeq	cmd_clear, r1, PRU_CLEAR
	qbeq	cmd_bus_clk_stop, r1, PRU_BUS_CLK_STOP
	qbeq	cmd_bus_clk_start, r1, PRU_BUS_CLK_START
	qbeq	cmd_TI_meas, r1, PRU_TI_MEAS
	qbeq	cmd_halt, r1, PRU_HALT

	// note that cmd_count() is run concurrently with command processing
	eget	r1, r.m, m.count
	qbeq	cmd_count, r1, PRU_COUNT
	jmp		cmd_get


// establish we're running by answering a ping
cmd_ping:
	eget	r1, r.m, m.p0
	eget	r2, r.m, m.p1
	add		r1, r1, r2
    eput	r1, r.m, m.p2
    eget	r1, r.m2, m2.m2_offset
    eput	r1, r.m, m.p3
	jmp 	cmd_done


// 5370 bus read
cmd_read:
	mov		r.p0, OFFSET(m.a_gen_0)
	jsr2	set_addr
	jsr1	bus_read
	eput	r.p0, r.m2, m2.d_gen_0
	eput	r.p1, r.m2, m2.d_gen_1
	eput	r.p2, r.m2, m2.d_gen_2
	jmp		cmd_done


// 5370 bus write
cmd_write:
	mov		r.p0, OFFSET(m.a_gen_0)
	jsr2	set_addr
	eget	r.p0, r.m2, m2.d_gen_0
	eget	r.p1, r.m2, m2.d_gen_0c
	eget	r.p2, r.m2, m2.d_gen_1
	eget	r.p3, r.m2, m2.d_gen_1c
	eget	r.p4, r.m2, m2.d_gen_2
	eget	r.p5, r.m2, m2.d_gen_2c
	jsr1	bus_write
	jmp		cmd_done


// clear overflow couters
cmd_clear:
	mov		r.n0_ovfl, 0
	eput	r.n0_ovfl, r.m, m.n0_ovfl
	mov		r.n3_ovfl, 0
	eput	r.n3_ovfl, r.m, m.n3_ovfl
	mov		r.ovfl_none, 0
	eput	r.ovfl_none, r.m, m.ovfl_none
	mov		r1, PRU_DONE
	eput	r1, r.m, m.count
	jmp 	cmd_done


// count N0/N3 counter overflows; stop when EOM (end-of-measurement) detected
cmd_count:
	eget	r1, r.m, m.watchdog
	add		r1, r1, 1
	eput	r1, r.m, m.watchdog

	jsr1	wait_bus_clk					// let bus clock run
	jsr1	addr_n0st
	jsr1	bus_read

check_n0:
	mov		r1, N0ST_N0_OVFL_GPIO			// on gpio1, active high
	and		r1, r.p1, r1
	qbeq	check_n3, r1, 0
	add		r.n0_ovfl, r.n0_ovfl, 1
	eput	r.n0_ovfl, r.m, m.n0_ovfl

	jsr1	addr_out3
	eget	r.p0, r.m, m.d_n0_clr_ovfl_0
	eget	r.p1, r.m, m.d_n0_clr_ovfl_0c
	eget	r.p2, r.m, m.d_n0_clr_ovfl_1
	eget	r.p3, r.m, m.d_n0_clr_ovfl_1c
	eget	r.p4, r.m, m.d_n0_clr_ovfl_2
	eget	r.p5, r.m, m.d_n0_clr_ovfl_2c
	jsr1	bus_write
	jmp		cmd_get

check_n3:
	mov		r1, N0ST_N3_OVFL_GPIO			// on gpio2, active high
	and		r1, r.p2, r1
	qbeq	check_eom, r1, 0
	add		r.n3_ovfl, r.n3_ovfl, 1
	eput	r.n3_ovfl, r.m, m.n3_ovfl

	jsr1	addr_out3
	eget	r.p0, r.m, m.d_n3_clr_ovfl_0
	eget	r.p1, r.m, m.d_n3_clr_ovfl_0c
	eget	r.p2, r.m, m.d_n3_clr_ovfl_1
	eget	r.p3, r.m, m.d_n3_clr_ovfl_1c
	eget	r.p4, r.m, m.d_n3_clr_ovfl_2
	eget	r.p5, r.m, m.d_n3_clr_ovfl_2c
	jsr1	bus_write
	jmp		cmd_get

check_eom:
	add		r.ovfl_none, r.ovfl_none, 1
	eput	r.ovfl_none, r.m, m.ovfl_none
	mov		r1, N0ST_EOM_GPIO				// on gpio1, active low
	and		r1, r.p1, r1
	qbne	cmd_get, r1, 0
	mov		r1, PRU_DONE
    eput	r1, r.m, m.count
	jmp		cmd_get


cmd_bus_clk_stop:
	// BUS_CLK_STOP()
	jsr2	bus_clock_stop_deassert
	jsr2	wait_clk
	jmp		cmd_done


cmd_bus_clk_start:
	// BUS_CLK_START()
	jsr2	wait_clk
	jsr2	bus_clock_start
	jmp		cmd_done


// perform a fast TI measurement
cmd_TI_meas:

	// FAST_WRITE_ENTER()
	jsr3	fast_write_enter

	// FAST_WRITE_GPIO_QUAL_CYCLE(ad_rst, 1,1,1,1,1,1,1,1)
	sgpio	r1, gpio0, ad_rst_0, ad_rst_0c
	sgpio	r1, gpio1, ad_rst_1, ad_rst_1c
	sgpio	r1, gpio2, ad_rst_2, ad_rst_2c
	sgpio	r1, gpio3, ad_rst_3, ad_rst_3c
	jsr3	bus_clock_pulse

	// FAST_WRITE_GPIO_QUAL_CYCLE(ad_ena, 1,1,1,1,1,1,1,1)
	sgpio	r1, gpio0, ad_ena_0, ad_ena_0c
	sgpio	r1, gpio1, ad_ena_1, ad_ena_1c
	sgpio	r1, gpio2, ad_ena_2, ad_ena_2c
	sgpio	r1, gpio3, ad_ena_3, ad_ena_3c
	jsr3	bus_clock_pulse

	// i.e. only gpio1 changes, gpio[023] same as previous write
	// FAST_WRITE_GPIO_QUAL_CYCLE(ad_arm, 0,0,1,1,0,0,0,0)
	sgpio	r1, gpio1, ad_arm_1, ad_arm_1c
	jsr3	bus_clock_pulse

	// FAST_WRITE_EXIT()
	jsr3	fast_write_exit

	// do {
	//     FAST_READ_GPIO1_CYCLE(1, n0st, a_n0st)
	// } while (n0st & N0ST_EOM_GPIO)
	jsr1	addr_n0st

wait_eom:
	jsr3	bus_read1
	mov		r1, N0ST_EOM_GPIO				// on gpio1, active low
	and		r1, r.p1, r1
	qbne	wait_eom, r1, 0
	
	// FAST_WRITE_ENTER()
	jsr3	fast_write_enter

	// FAST_WRITE_GPIO_CYCLE(ad_idle)
	sgpio	r1, gpio0, ad_idle_0, ad_idle_0c
	sgpio	r1, gpio1, ad_idle_1, ad_idle_1c
	sgpio	r1, gpio2, ad_idle_2, ad_idle_2c
	sgpio	r1, gpio3, ad_idle_3, ad_idle_3c
	jsr3	bus_clock_pulse

	// FAST_WRITE_EXIT()
	jsr3	fast_write_exit

	// FAST_READ_GPIO_CYCLE(a_n0st, n0st)
	jsr1	addr_n0st
	jsr3	bus_read1
	eput	r.p0, r.m2, m2.d_n0st_0
	eput	r.p1, r.m2, m2.d_n0st_1
	eput	r.p2, r.m2, m2.d_n0st_2

	// FAST_READ2_GPIO_CYCLE(a_n1n2h, n1n2_h, n1n2_l)
	mov		r.p0, OFFSET(m.a_n1n2_h_0)
	jsr2	set_addr
	jsr3	bus_read1
	eput	r.p0, r.m2, m2.d_n1n2_h_0
	eput	r.p1, r.m2, m2.d_n1n2_h_1
	eput	r.p2, r.m2, m2.d_n1n2_h_2
	mov		r1, BUS_LA0						// on gpio3, active low
	st32	r1, r.gpio3, r.gpio_clr
	eput	r.p0, r.m2, m2.d_n1n2_l_0
	eput	r.p1, r.m2, m2.d_n1n2_l_1
	eput	r.p2, r.m2, m2.d_n1n2_l_2

	// FAST_READ2_GPIO_CYCLE(a_n0h, n0_h, n0_l)
	mov		r.p0, OFFSET(m.a_n0_h_0)
	jsr2	set_addr
	jsr3	bus_read1
	eput	r.p0, r.m2, m2.d_n0_h_0
	eput	r.p1, r.m2, m2.d_n0_h_1
	eput	r.p2, r.m2, m2.d_n0_h_2
	mov		r1, BUS_LA0						// on gpio3, active low
	st32	r1, r.gpio3, r.gpio_clr
	eput	r.p0, r.m2, m2.d_n0_l_0
	eput	r.p1, r.m2, m2.d_n0_l_1
	eput	r.p2, r.m2, m2.d_n0_l_2

	jmp		cmd_done


// wait for Beagle LEDs to go dark for 1 sec,
// then blank 5370 display to let user know safe to power-off
cmd_halt:
	mov		r2, 10000000
cmd_halt1:
	ld32	r1, r.gpio1, r.gpio_out
	mov		r0, GPIO1_LEDS
	and		r0, r1, r0
	qbne	cmd_halt, r0, 0
	sub		r2, r2, 1
	qbne	cmd_halt1, r2, 0

	mov		r5, 15
cmd_halt2:
	jsr4	dsp_chr
	sub		r5, r5, 1
	qbne	cmd_halt2, r5, 0

	jmp		cmd_done

dsp_chr:
	eget	r0, r.m, m.a_gen_0
	eget	r1, r.m, m.a_gen_0c

	mov		r3, BUS_LA2
	clr		r0, r0, 14
	clr		r1, r1, 14
	
	and		r2, r5, 4
	qbeq	dsp_la2z, r2, 0
	or		r0, r0, r3
	jmp		dsp_la2
dsp_la2z:
	or		r1, r1, r3
dsp_la2:

	mov		r3, BUS_LA3
	clr		r0, r0, 15
	clr		r1, r1, 15
	
	and		r2, r5, 8
	qbeq	dsp_la3z, r2, 0
	or		r0, r0, r3
	jmp		dsp_la3
dsp_la3z:
	or		r1, r1, r3
dsp_la3:

	st32	r0, r.gpio0, r.gpio_clr			// set/clr order reversed for LAn
	st32	r1, r.gpio0, r.gpio_set

	eget	r0, r.m, m.a_gen_3
	eget	r1, r.m, m.a_gen_3c

	mov		r3, BUS_LA0
	clr		r0, r0, 16
	clr		r1, r1, 16
	
	and		r2, r5, 1
	qbeq	dsp_la0z, r2, 0
	or		r0, r0, r3
	jmp		dsp_la0
dsp_la0z:
	or		r1, r1, r3
dsp_la0:

	mov		r3, BUS_LA1
	clr		r0, r0, 19
	clr		r1, r1, 19
	
	and		r2, r5, 2
	qbeq	dsp_la1z, r2, 0
	or		r0, r0, r3
	jmp		dsp_la1
dsp_la1z:
	or		r1, r1, r3
dsp_la1:

	st32	r0, r.gpio3, r.gpio_clr			// set/clr order reversed for LAn
	st32	r1, r.gpio3, r.gpio_set

	eget	r.p0, r.m2, m2.d_gen_0
	eget	r.p1, r.m2, m2.d_gen_0c
	eget	r.p2, r.m2, m2.d_gen_1
	eget	r.p3, r.m2, m2.d_gen_1c
	eget	r.p4, r.m2, m2.d_gen_2
	eget	r.p5, r.m2, m2.d_gen_2c
	jsr1	bus_write

	rtn4


// ---------------------------

// call with: address already set
// returns: r.p0=gpio0 r.p1=gpio1 r.p2=gpio2
bus_read:
	// BUS_CLK_STOP()
	jsr2	bus_clock_stop_deassert
	jsr2	wait_clk
	
	jsr3	bus_read1

	// BUS_CLK_START()
	jsr2	wait_clk
	jsr2	bus_clock_start
	rtn1

bus_read1:
	
	// FAST_READ_CYCLE()
	mov		r1, BUS_DIR
	st32	r1, r.gpio0, r.gpio_set
	mov		r1, BUS_LRW
	st32	r1, r.gpio0, r.gpio_clr
	mov		r1, BUS_LVMA
	st32	r1, r.gpio1, r.gpio_clr
	jsr2	wait_clk
	jsr2	bus_clock_assert
	
	jsr2	wait_access
	ld32	r.p0, r.gpio0, r.gpio_in
	not		r.p0, r.p0						// invert LDn
	ld32	r.p1, r.gpio1, r.gpio_in
	not		r.p1, r.p1
	ld32	r.p2, r.gpio2, r.gpio_in
	not		r.p2, r.p2
	
	jsr2	bus_clock_stop_deassert
	mov		r1, BUS_LVMA
	st32	r1, r.gpio1, r.gpio_set
	rtn3


// call with:
//		address already set
//		r.p0=gpio0_set r.p1=gpio0_clr
//		r.p2=gpio1_set r.p3=gpio1_clr
//		r.p4=gpio2_set r.p5=gpio2_clr
bus_write:
	// BUS_CLK_STOP()
	jsr2	bus_clock_stop_deassert
	jsr2	wait_clk

	// FAST_WRITE_ENTER()
	jsr3	fast_write_enter

	// FAST_WRITE_CYCLE()
	st32	r.p0, r.gpio0, r.gpio_clr		// data is active low, so swap set/clr sense
	st32	r.p1, r.gpio0, r.gpio_set
	st32	r.p2, r.gpio1, r.gpio_clr
	st32	r.p3, r.gpio1, r.gpio_set
	st32	r.p4, r.gpio2, r.gpio_clr
	st32	r.p5, r.gpio2, r.gpio_set
	jsr3	bus_clock_pulse

	// FAST_WRITE_EXIT()
	jsr3	fast_write_exit

	// BUS_CLK_START()
	jsr2	wait_clk
	jsr2	bus_clock_start
	rtn1


fast_write_enter:

	// FAST_WRITE_ENTER()
	mov		r1, BUS_DIR
	st32	r1, r.gpio0, r.gpio_clr

	mov		r0, (~G0_DATA)
	mov		r1, 0
	jsr2	set_oe0
	mov		r0, (~G1_DATA)
	mov		r1, 0
	jsr2	set_oe1
	mov		r0, (~G2_DATA)
	mov		r1, 0
	jsr2	set_oe2

	mov		r1, BUS_LRW
	st32	r1, r.gpio0, r.gpio_set
	mov		r1, BUS_LVMA
	st32	r1, r.gpio1, r.gpio_clr
	rtn3


fast_write_exit:

	// FAST_WRITE_EXIT()
	mov		r1, BUS_LRW
	st32	r1, r.gpio0, r.gpio_clr
	mov		r1, BUS_LVMA
	st32	r1, r.gpio1, r.gpio_set

	mov		r0, 0xffffffff
	mov		r1, G0_DATA
	jsr2	set_oe0
	mov		r0, 0xffffffff
	mov		r1, G1_DATA
	jsr2	set_oe1
	mov		r0, 0xffffffff
	mov		r1, G2_DATA
	jsr2	set_oe2

	mov		r1, BUS_DIR
	st32	r1, r.gpio0, r.gpio_set
	rtn3


// r.p0 = OFFSET(in r.m of address values)
set_addr:
	ld32	r1, r.m, r.p0
	add		r.p0, r.p0, 4
	st32	r1, r.gpio0, r.gpio_clr			// set/clr order reversed for LAn

	ld32	r1, r.m, r.p0
	add		r.p0, r.p0, 4
	st32	r1, r.gpio0, r.gpio_set

	ld32	r1, r.m, r.p0
	add		r.p0, r.p0, 4
	st32	r1, r.gpio1, r.gpio_clr

	ld32	r1, r.m, r.p0
	add		r.p0, r.p0, 4
	st32	r1, r.gpio1, r.gpio_set

	ld32	r1, r.m, r.p0
	add		r.p0, r.p0, 4
	st32	r1, r.gpio3, r.gpio_clr

	ld32	r1, r.m, r.p0
	add		r.p0, r.p0, 4
	st32	r1, r.gpio3, r.gpio_set
	rtn2

addr_n0st:
	mov		r.p0, OFFSET(m.a_n0st_0)
	jsr2	set_addr
	rtn1

addr_out3:
	mov		r.p0, OFFSET(m.a_o3_0)
	jsr2	set_addr
	rtn1

set_oe0:
	ld32	r3, r.gpio0, r.gpio_oe
	and		r3, r3, r0
	or		r3, r3, r1
	st32	r3, r.gpio0, r.gpio_oe
	rtn2

set_oe1:
	ld32	r3, r.gpio1, r.gpio_oe
	and		r3, r3, r0
	or		r3, r3, r1
	st32	r3, r.gpio1, r.gpio_oe
	rtn2

set_oe2:
	ld32	r3, r.gpio2, r.gpio_oe
	and		r3, r3, r0
	or		r3, r3, r1
	st32	r3, r.gpio2, r.gpio_oe
	rtn2
	
wait_clk:
	// 100 ns
	mov		r1, 10
wait_clk1:
	sub		r1, r1, 1
	qbne	wait_clk1, r1, 0
	rtn2
	
wait_access:
	// 800 ns
	mov		r1, 80
wait_access1:
	sub		r1, r1, 1
	qbne	wait_access1, r1, 0
	rtn2

wait_bus_clk:
	// 50 us
	mov		r1, 5000
wait_bus_clk1:
	sub		r1, r1, 1
	qbne	wait_bus_clk1, r1, 0
	rtn2
	
wait_250ms:
	mov		r1, 25000000
wait_250ms_1:
	sub		r1, r1, 1
	qbne	wait_250ms_1, r1, 0
	rtn2
	
bus_clock_stop_deassert:
	mov		r1, T_MODE
	st32	r1,	r.timer, r.timer_tclr
	rtn2
	
bus_clock_assert:
	mov		r1, (T_MODE | T_HIGH)
	st32	r1,	r.timer, r.timer_tclr
	rtn2
	
bus_clock_start:
	mov		r1, -9
	st32	r1,	r.timer, r.timer_tcrr
	mov		r1, (T_MODE | T_START)
	st32	r1,	r.timer, r.timer_tclr
	rtn2

bus_clock_pulse:
	jsr2	wait_access
	jsr2	bus_clock_assert
	jsr2	wait_access
	jsr2	bus_clock_stop_deassert
	jsr2	wait_clk
	rtn3
