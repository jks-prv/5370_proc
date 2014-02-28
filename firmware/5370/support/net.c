#include "boot.h"
#include "misc.h"
#include "net.h"
#include "hpib.h"

#include <sys/file.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <setjmp.h>
#include <ctype.h>
#include <signal.h>

#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "printf.h"

static int sfd[N_NET_CONN], srv_sock[N_NET_CONN];

int net_connect(net_conn_t t, net_cs_t cs, char *host, int port)
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s;
	struct sigaction sig;
	char port_str[16];
	
	memset(&sig, 0, sizeof(struct sigaction));
	sig.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sig, NULL);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	//hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
	hints.ai_socktype = SOCK_STREAM; /* TCP socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;          /* Any protocol */
	
	if (cs == SERVER) {
		int val = 1;
		struct	sockaddr_in	addr;
		if ((sfd[t] = socket(PF_INET, SOCK_STREAM, 0)) < 0) sys_panic("server socket");
		setsockopt(sfd[t], SOL_SOCKET, SO_REUSEADDR, &val, sizeof val);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);
		
		if (bind(sfd[t], (struct sockaddr*) &addr, sizeof addr) < 0) {
			if (errno == EADDRINUSE) {
				lprintf("network port %d in use\n", port);
				lprintf("app already running in background?\ntry \"make stop\" (or \"m stop\") first\n");
				xit(0);
			}
			sys_panic("bind");
		}
		
		if (listen(sfd[t], 1) < 0) sys_panic("listen");
		if (fcntl(sfd[t], F_SETFL, O_NONBLOCK) < 0) sys_panic("socket non-block");
		lprintf("listening for TCP connection on port %d..\n", port);
		return sfd[t];
	}
	
	// cs == CLIENT
	sprintf(port_str, "%d", port);
	s = getaddrinfo(host, port_str, &hints, &result);

	if (s != 0) {
	   fprintf(stderr, "getaddrinfo: \"%s\" %s\n", host, gai_strerror(s));
	   fprintf(stderr, "Check /etc/hosts?\n");
	   panic("getaddrinfo");
	}
	
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd[t] = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		//printf("socket: sfd %d fam %d type %d protocol %d addr 0x%x\n", sfd[t],
		//rp->ai_family, rp->ai_socktype, rp->ai_protocol, rp->ai_addr);

		if (sfd[t] == -1)
			continue;
	
		if (connect(sfd[t], rp->ai_addr, rp->ai_addrlen) != -1)
			break;
		
		close(sfd[t]);
	}
	
	if (rp == NULL) {
	   fprintf(stderr, "Could not connect to \"%s\"\n", host);
	   exit(-1);
	}
	
	freeaddrinfo(result);

	if (fcntl(sfd[t], F_SETFL, O_NONBLOCK) < 0) sys_panic("socket non-block");

	printf("connected to %s\n", host);	
	return sfd[t];
}

void net_disconnect(net_conn_t t)
{
	if (sfd[t]) close(sfd[t]);
	if (srv_sock[t]) close(srv_sock[t]);
	srv_sock[t] = 0;
}

#ifdef CLIENT_SIDE

int net_client_read(int sfd, char *ib, int len, bool read_once)
{
	int n, rem = len;
	
	while (rem) {
		n = read(sfd, ib, rem);
		if (n < 0) {
			if (errno == EAGAIN) {
				continue;
			}
			sys_panic("net_client_read");
		}
		if (read_once) return n;
		ib += n;
		rem -= n;
		assert(rem == 0);
	}

	return len;
}

#else

static char rx_buf[N_NET_CONN][HPIB_PKT_BUFSIZE];	

u4_t net_poll(net_conn_t t, char **nb)
{
	int n;
	struct addrinfo ai;
	
	if (!srv_sock[t]) {
		memset(&ai, 0, sizeof(struct addrinfo));
		if ((n = accept(sfd[t], ai.ai_addr, &ai.ai_addrlen)) < 0) {
			if (errno == EWOULDBLOCK) return 0;
			sys_panic("net_poll accept");
		}

		if (fcntl(n, F_SETFL, O_NONBLOCK) < 0) sys_panic("net_poll non-block");
		srv_sock[t] = n;
		printf("connected\n");
	}
	
	if ((n = read(srv_sock[t], &rx_buf[t][0], HPIB_PKT_BUFSIZE)) < 0) {
		if (errno == EAGAIN) return 0;
		sys_panic("net_poll read");
	}

	if (n) {
		*nb = &rx_buf[t][0];
	} else {
		close(srv_sock[t]);
		srv_sock[t] = 0;
		printf("connection closed, listening..\n");
		n = 0;
	}
	
	return n;
}

bool net_no_connection(net_conn_t t)
{
	return ((!srv_sock[t])? TRUE:FALSE);
}

static u1_t tx_buf[N_NET_CONN][HPIB_PKT_BUFSIZE];

u1_t *net_send(net_conn_t t, char *cb, u4_t nb, bool no_copy, bool flush)
{
	static int send_idx;
	char *buf = (char *) &tx_buf[t][0];
	int cnt = send_idx;
	
	if (!srv_sock[t]) return 0;

	if (nb) {
		if (no_copy) {
			buf = cb;
			cnt = nb;
		} else {
			if ((send_idx + nb) >= HPIB_PKT_BUFSIZE) panic("net_send size");
			bcopy(cb, (char *) &tx_buf[t][send_idx], nb);
			send_idx += nb;
			cnt = send_idx;
		}
	}

	if (flush && cnt) {
		if (!srv_sock[t]) {
			if (fwrite(buf, 1, cnt, stdout) != cnt) {
				sys_panic("net_send fwrite");
			}
			fflush(stdout);
		} else
		if (write(srv_sock[t], buf, cnt) < 0) {
			if ((errno == EPIPE) || (errno == ECONNRESET)) return 0;
			sys_panic("net_send write");
		}
		send_idx = 0;
	}
	
	return &tx_buf[t][0];
}

#endif
