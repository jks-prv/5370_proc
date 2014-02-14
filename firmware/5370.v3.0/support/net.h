#ifndef _NET_H_
#define _NET_H_

typedef enum { CLIENT, SERVER } net_type_e;

int net_connect(net_type_e cs, char *host, int port);
void net_disconnect();
int net_client_read(int sfd, char *ib, int len, bool read_once);
void net_poll();
bool net_no_connection();

// just documentation
#define	NO_COPY(x) x
#define	FLUSH(x) x

u4_t *net_send(char *cb, u4_t nb, bool no_copy, bool flush);

#endif
