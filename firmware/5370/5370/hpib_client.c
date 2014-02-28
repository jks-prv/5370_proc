// an example of how to open a tcp connection to instrument and perform & decode a binary transfer

#include "misc.h"
#include "5370.h"
#include "hpib.h"
#include "net.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

static char buf[HPIB_PKT_BUFSIZE];

void net_hpib(int sfd, char *cmd, bool reply)
{
	int n;
	
	sprintf(buf, "%s\r\n", cmd);
	write(sfd, buf, strlen(buf));

	if (reply) {
		n = net_client_read(sfd, buf, 64, TRUE);
		buf[n] = 0;
		printf("%s", buf);
	}
}

// measured transfer statistics:
// original 5370 binary mode over HPIB: about 6000 meas/sec

// for 5370 v3 board (the one with Beaglebone Black) compiled with -O3 optimization:
// 5-byte per transfer regular mode: about 12000 meas/sec
// 2-byte per transfer fast mode: about 39000 meas/sec

// loop counts for 10 seconds of data transfer
#define NMEAS_10_SEC_REG	120000
#define NLOOP		(NMEAS_10_SEC_REG / HPIB_MEAS_PER_PKT)
#define NMEAS_10_SEC_FAST	390000
#define NLOOP_FAST	(NMEAS_10_SEC_FAST / HPIB_MEAS_PER_FAST_PKT)

// measure with and without the overhead of the post-transfer calculation
#define DO_CALC

//#define HOST	"hp5370a.2217a01627"
#define HOST	"localhost"

bool background_mode = FALSE;

int main(int argc, char *argv[])
{
	int i, j, sfd, e;
	char *bp;
	u1_t *s, *s2;
	bool rng, fast_mode = FALSE;
	
	i=1;
	if ((i < argc) && (strcmp(argv[i], "-f") == 0)) { fast_mode = TRUE; i++; }
	
	sfd = net_connect(NET_HPIB, CLIENT, (i < argc)? argv[i]:HOST, HPIB_TCP_PORT);

	if (fast_mode) {

#ifndef DO_CALC
	printf("WARNING: remember, calculation currently compiled out\n");
#endif

		net_hpib(sfd, "tb2", FALSE);		// tb2 is a virtual cmd we define for fast mode
		
		for (i=0; i<NLOOP_FAST; i++) {
			s4_t n1n2;
			double ti;
		
			bp = buf;
			s = (u1_t *) buf;
			net_client_read(sfd, bp, HPIB_BYTES_PER_PKT, FALSE);

#ifdef DO_CALC
			for (j=0; j<HPIB_MEAS_PER_FAST_PKT; j++) {

				// ARM is little-endian
				n1n2 = (s4_t) ((s[1] << 8) | (u1_t) s[0]);
				s += 2;
				ti = ((double) n1n2 / 256.0) * 5.0e-9;
				rng = ((ti < 98.0e-9) || (ti > 99.9e+9))? TRUE:FALSE;
				if (rng) printf("%d: out of range:\n", j);

				if (rng) {
					if (ti < 1.0e-8) {
						printf("%1.2f ns\n", ti * 1.0e8);
					} else {
						printf("%2.2f ns\n", ti * 1.0e9);
					}
				}
			}
#endif
		}
	} else {
	
		// take a couple of measurements the regular way
		net_hpib(sfd, "md2", TRUE);
		net_hpib(sfd, "mr", TRUE);
		
		// switch to binary mode
		net_hpib(sfd, "tb1", FALSE);

		for (i=0; i<NLOOP; i++) {
			u4_t n;
			u1_t n0st, n1n2h, n1n2l, n0h, n0l;
			s4_t n1n2, n0;
			double ti;
			
			bp = buf;
			s = (u1_t *) buf;
			net_client_read(sfd, bp, HPIB_BYTES_PER_PKT, FALSE);
			
			for (j=0; j<HPIB_MEAS_PER_PKT; j++) {
				e=0;
				n0st = s[0]; n1n2h = s[1]; n1n2l = s[2]; n0h = s[3]; n0l = s[4];
				s += 5;
				if (isActive(N0ST_PLL_OOL, n0st)) { printf("PLL UNLOCKED\n"); e=1; }
				if (isActive(N0ST_N0_OVFL, n0st)) { printf("N0 OVFL\n"); e=1; }
				
				n1n2 = ((n0st & N0ST_N1N2) << 16) | (n1n2h << 8) | n1n2l;
				
				// convert from 18-bit 2s complement
				if (n1n2 >= 0x20000) {
					n1n2 = n1n2 - 0x40000;
				}
				
				n0 = (n0h << 8) | n0l;
		
				if ((n0st & N0ST_N0_POS) == 0) {
					n0 = -n0;
				}
				
				ti = (((double) n1n2 / 256.0) + (double) n0) * 5.0e-9;
				rng = ((ti < 99.0e-9) || (ti > 99.9e+9))? TRUE:FALSE;
				if (rng) { printf("out of range: 0x%02x\n", n0st & N0ST_STATUS); e=1; }

				if (rng) {
					if (ti < 1.0e-8) {
						printf("%1.2f ns\n", ti * 1.0e8);
					} else {
						printf("%2.2f ns\n", ti * 1.0e9);
					}
				}
				
				if (e) {
					s2 = (u1_t *) &buf[5];
					printf("%d %d/%d:  0x%x %d %d %d %d  0x%x %d %d %d %d\n", i, j, HPIB_MEAS_PER_PKT,
						s[-5], s[-4], s[-3], s[-2], s[-1],
						s2[-5], s2[-4], s2[-3], s2[-2], s2[-1]);
				}
			}
		}
	}

	// switch out of binary mode
	net_hpib(sfd, "tb0md1", FALSE);
}
