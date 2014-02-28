#ifndef _NET_H_
#define _NET_H_

typedef enum { CLIENT, SERVER } net_cs_t;

#define N_NET_CONN 2
typedef enum { NET_HPIB, NET_TELNET } net_conn_t;

int net_connect(net_conn_t t, net_cs_t cs, char *host, int port);
void net_disconnect(net_conn_t t);
int net_client_read(int sfd, char *ib, int len, bool read_once);
u4_t net_poll(net_conn_t t, char **nb);
bool net_no_connection(net_conn_t t);

// just documentation
#define	NO_COPY(x) x
#define	FLUSH(x) x

u1_t *net_send(net_conn_t t, char *cb, u4_t nb, bool no_copy, bool flush);

#endif
