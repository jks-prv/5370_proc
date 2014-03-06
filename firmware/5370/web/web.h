#ifndef _SRV_H_
#define _SRV_H_

#define WEBSERVER_PORT	"5372"

#define	INDEX_HTML		"5370.html"

void app_to_webserver(char *s, int sl);
int webserver_to_app(char *s, int sl);

void web_server_start();
void web_server_stop();
void web_server_init();

// in mongoose.c but not mongoose.h
void *ns_start_thread(void *(*f)(void *), void *p);

#endif
