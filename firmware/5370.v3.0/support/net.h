#ifndef _NET_H_
#define _NET_H_

typedef enum { CLIENT, SERVER } net_type_e;

int net_connect(net_type_e cs, char *host, char *port);
int net_client_read(int sfd, char *ib, int len, bool read_once);

void net_poll();
u4_t *net_send(char *cb, u4_t nb, bool no_copy, bool flush);

#endif
