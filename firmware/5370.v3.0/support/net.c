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

static int sfd, srv_sock, listening_port;

int net_connect(net_type_e cs, char *host, int port)
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
		if ((sfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) sys_panic("server socket");
		setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof val);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);
		
		if (bind(sfd, (struct sockaddr*) &addr, sizeof addr) < 0) {
			if (errno == EADDRINUSE) {
				lprintf("network port %d in use\n", port);
				lprintf("app already running in background?\ntry \"make stop\" (or \"m stop\") first\n");
				xit(0);
			}
			sys_panic("bind");
		}
		
		if (listen(sfd, 1) < 0) sys_panic("listen");
		if (fcntl(sfd, F_SETFL, O_NONBLOCK) < 0) sys_panic("socket non-block");
		lprintf("listening for TCP connection on port %d..\n", port);
		listening_port = port;
		return sfd;
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
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		//printf("socket: sfd %d fam %d type %d protocol %d addr 0x%x\n", sfd,
		//rp->ai_family, rp->ai_socktype, rp->ai_protocol, rp->ai_addr);

		if (sfd == -1)
			continue;
	
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;
		
		close(sfd);
	}
	
	if (rp == NULL) {
	   fprintf(stderr, "Could not connect to \"%s\"\n", host);
	   exit(-1);
	}
	
	freeaddrinfo(result);

	if (fcntl(sfd, F_SETFL, O_NONBLOCK) < 0) sys_panic("socket non-block");

	printf("connected to %s\n", host);	
	return sfd;
}

#ifdef CLIENT_SIDE

int net_client_read(int sfd, char *ib, int len, bool read_once)
{
	int n, rem = len;
	static char nbuf[HPIB_PKT_BUFSIZE];	
	
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

void net_poll()
{
	int n;
	struct addrinfo ai;
	static char nbuf[HPIB_PKT_BUFSIZE];	
	
	if (!srv_sock) {
		memset(&ai, 0, sizeof(struct addrinfo));
		if ((n = accept(sfd, ai.ai_addr, &ai.ai_addrlen)) < 0) {
			if (errno == EWOULDBLOCK) return;
			sys_panic("net_poll accept");
		}

		if (fcntl(n, F_SETFL, O_NONBLOCK) < 0) sys_panic("net_poll non-block");
		srv_sock = n;
		printf("incoming connection\n");
	} else {
		if ((n = read(srv_sock, nbuf, HPIB_PKT_BUFSIZE)) < 0) {
			if (errno == EAGAIN) return;
			sys_panic("net_poll read");
		}

		if (n) {
#ifdef HPIB_SIM
			hpib_input(nbuf);
#endif
		} else {
			close(srv_sock);
			srv_sock = 0;
			printf("connection closed, listening..\n");
		}
	}
}

bool net_no_connection()
{
	return ((!srv_sock)? TRUE:FALSE);
}

u4_t *net_send(char *cb, u4_t nb, bool no_copy, bool flush)
{
	static u4_t send_buf_aligned[HPIB_PKT_BUFSIZE/4];
	static int send_idx;
	char *buf = (char *) send_buf_aligned;
	int cnt = send_idx;

	if (nb) {
		if (no_copy) {
			buf = cb;
			cnt = nb;
		} else {
			if ((send_idx + nb) >= HPIB_PKT_BUFSIZE) panic("net_send size");
			bcopy(cb, &((char *) send_buf_aligned)[send_idx], nb);
			send_idx += nb;
			cnt = send_idx;
		}
	}

	if (flush && cnt) {
		if (!srv_sock) {
			if (fwrite(buf, 1, cnt, stdout) != cnt) {
				sys_panic("net_send fwrite");
			}
			fflush(stdout);
		} else
		if (write(srv_sock, buf, cnt) < 0) {
			if ((errno == EPIPE) || (errno == ECONNRESET)) return 0;
			sys_panic("net_send write");
		}
		send_idx = 0;
	}
	
	return send_buf_aligned;
}

#endif
