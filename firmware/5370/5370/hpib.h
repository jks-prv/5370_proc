#ifndef _HPIB_H_
#define _HPIB_H_


// read-only registers
#define R0_data_in		ADDR_HPIB(0)	// hpib data in
#define	R1_state		ADDR_HPIB(1)
#define	R2_state		ADDR_HPIB(2)
#define	R3_status		ADDR_HPIB(3)

// R1_state
#define H_data		0x01
#define H_rdy		0x02
#define H_rmt_qual	0x04
#define H_com		0x08
#define H_EOI		0x10
#define H_IRQ		0x80

// R2_state
#define H_cmdROM	0x07
#define H_Nlisten	0x08

// R3_status
#define H_listen	0x01
#define H_talk		0x02
#define H_poll		0x04
#define H_rmt		0x08
#define H_ATN		0x10
#define H_HRFD_i	0x20


// write-only registers
#define	W0_data_out		(ADDR_HPIB(0)+4)	// hpib data out
#define	W1_status		(ADDR_HPIB(1)+4)	// hpib status out
#define	W2_ctrl			(ADDR_HPIB(2)+4)
#define	W3_not_used		(ADDR_HPIB(3)+4)

// W1_status
#define H_LSRQ_o	0x40

// W2_ctrl
#define H_ifc_sw	0x01
#define H_LIRQ_ENL	0x02
#define H_LNMI_ENL	0x04
#define H_ren_sw	0x08
#define H_EOI_o		0x10	// not LEOI because no inverter in path
#define H_rdy_sw	0x20

/*

interface management signals:
	L ATN i		attention (DIO is a cmd)
	L IFC i		interface clear
	L REN i		remote enable
	L SRQ o		service request (interrupt to controller)
	L EOI io	end-or-identify

handshake signals:
	L DAV io	data valid
	H NRFD io	not ready-for-data
	H NDAC io	not data accepted

side effects of HPIB register access:
	R0_data_in	clocks data FF
	R1_state	clocks irq FF
	R2_state	clocks com FF
	R3_status	clocks rmt-qual FF
	
	W0_data_out	clocks rdy FF
	W1_status	clocks srq FF

*/


// for transfers of HPIB measurement data over the network
#define HPIB_PKT_BUFSIZE	(1048-1)

#define HPIB_BYTES_PER_PKT		1000
#define HPIB_BYTES_PER_MEAS		5
#define HPIB_MEAS_PER_PKT		(HPIB_BYTES_PER_PKT / HPIB_BYTES_PER_MEAS)

#define HPIB_BYTES_PER_FAST_MEAS	2
#define HPIB_MEAS_PER_FAST_PKT		(HPIB_BYTES_PER_PKT / HPIB_BYTES_PER_FAST_MEAS)

extern bool hpib_causeIRQ, hps;

void hpib_args(bool cmd_line, int argc, char *argv[]);
void hpib_reset();
u4_t hpib_recv(void *conn, char *buf);
void hpib_input(char *buf, u4_t n);
u4_t hpib_fast_binary(s2_t *ibp, u4_t nloop);
u1_t handler_dev_hpib_read(u2_t addr);
void handler_dev_hpib_write(u2_t addr, u1_t d);

#endif
