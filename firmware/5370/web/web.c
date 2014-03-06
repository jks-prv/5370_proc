#include <string.h>
#include <time.h>
#include <pthread.h>

#include "boot.h"
#include "web.h"
#include "mongoose.h"

#if 0
 #define DBG(...) printf(__VA_ARGS__)
#else
 #define DBG(...)
#endif

#ifdef _WIN32
#define snprintf _snprintf
#endif

#define MIN(a,b)	( ((a)<=(b))? (a):(b) )

extern const char *find_embedded_file(const char *, size_t *);

pthread_mutex_t a2w_lock, w2a_lock;

typedef struct nbuf {
	char *buf;
	u2_t len;
	u1_t done;
	u1_t n;
	struct nbuf *next, *prev;
} nbuf_t;

nbuf_t *a2w = NULL, *a2w_head = NULL, *w2a = NULL, *w2a_head = NULL;

static void nbuf_enqueue(pthread_mutex_t *lock, nbuf_t *nb, nbuf_t **q, nbuf_t **q_head)
{
	nbuf_t *dp;
	
	pthread_mutex_lock(lock);
	
		// collect done buffers (same thread where they're allocated)
		while ((dp = *q_head) && dp->done) {
			assert(dp->buf); free(dp->buf);
			*q_head = dp->prev;
			if (dp == *q) *q = NULL;
			free(dp);
		}
		
		if (*q) (*q)->prev = nb;
		nb->next = *q;
		*q = nb;
		nb->prev = NULL;
		if (!*q_head) *q_head = nb;
	pthread_mutex_unlock(lock);
}

static void nbuf_allocq(pthread_mutex_t *lock, char *s, int sl, nbuf_t **q, nbuf_t **q_head)
{
	nbuf_t *nb;
	
	assert(s && sl);
	nb = (nbuf_t*) malloc(sizeof(nbuf_t));
	nb->buf = (char*) malloc(sl);
	memcpy(nb->buf, s, sl);
	nb->len = sl;
	nb->done = FALSE;
	nbuf_enqueue(lock, nb, q, q_head);
}

static nbuf_t *nbuf_dequeue(pthread_mutex_t *lock, nbuf_t **q, nbuf_t **q_head)
{
	nbuf_t *nb;
	
	pthread_mutex_lock(lock);
		nb = *q_head;
		while (nb && nb->done) {
			nb = nb->prev;
		}
	pthread_mutex_unlock(lock);
	
	assert(!nb || (nb && !nb->done));
	return nb;
}


// browser => (websocket) => webserver => (mutex memcpy) => app

static int request(struct mg_connection *conn) {
	size_t index_size=0;
	const char *index_html;

	if (conn->is_websocket) {
		// This handler is called for each incoming websocket frame, one or more
		// times for connection lifetime.
		char *s = conn->content;
		int sl = conn->content_len;
		if (sl == 0) return MG_TRUE;	// keepalive?
		
		DBG("WEBSOCKET: %d <%s> ", sl, s);
		nbuf_allocq(&w2a_lock, s, sl, &w2a, &w2a_head);
		
		return conn->content_len == 4 && !memcmp(conn->content, "exit", 4) ? MG_FALSE : MG_TRUE;
	} else {
		if (strcmp(conn->uri, "/") == 0) conn->uri = INDEX_HTML; else
		if (conn->uri[0] == '/') conn->uri++;
		index_html = find_embedded_file(conn->uri, &index_size);
		DBG("DATA: %s %d ", conn->uri, (int) index_size);
		mg_send_header(conn, "Content-Type", "text/html");
		mg_send_data(conn, index_html, index_size);
		return MG_TRUE;
	}
}

static int ev_handler(struct mg_connection *conn, enum mg_event ev) {
  int r;
  
  if (ev == MG_REQUEST) {
  	DBG("ev_handler:MG_REQUEST: URI:%s query:%s ", conn->uri, conn->query_string);
    r = request(conn);
    DBG("\n");
    return r;
  } else if (ev == MG_AUTH) {
  	DBG("ev_handler:MG_AUTH\n");
    return MG_TRUE;
  } else {
  	DBG("ev_handler:OTHER: <%s> %d\n", conn->content, (int) conn->content_len);
    return MG_FALSE;
  }
}

int webserver_to_app(char *s, int sl)
{
	nbuf_t *nb;
	
	nb = nbuf_dequeue(&w2a_lock, &w2a, &w2a_head);
	if (!nb) return 0;
	assert(!nb->done && nb->buf && nb->len);
	memcpy(s, nb->buf, MIN(nb->len, sl));
	nb->done = TRUE;
	sched_yield();
	
	return nb->len;
}


// app => (mutex memcpy) => webserver => (websocket) => browser

void app_to_webserver(char *s, int sl)
{
	nbuf_allocq(&a2w_lock, s, sl, &a2w, &a2w_head);
	sched_yield();
}

static int iterate_callback(struct mg_connection *c, enum mg_event ev)
{
	nbuf_t *nb;
	
	if (ev == MG_POLL && c->is_websocket) {

		while (TRUE) {
			nb = nbuf_dequeue(&a2w_lock, &a2w, &a2w_head);
			
			if (nb) {
				assert(!nb->done && nb->buf && nb->len);
				DBG("iterate_callback:WEBSOCKET: %d <%s>\n", nb->len, nb->buf);
				mg_websocket_write(c, 1, nb->buf, nb->len);
				nb->done = TRUE;
			} else {
				break;
			}
		}
	} else {
		DBG("iterate_callback:OTHER: %d\n", (int) c->content_len);
	}
	
	return MG_TRUE;
}

static bool stopServer = FALSE;

void *web_server(void *none)
{
  struct mg_server *server = mg_create_server(NULL, ev_handler);
  u4_t current_timer = 0, last_timer = timer_ms();

  mg_set_option(server, "listening_port", WEBSERVER_PORT);
  printf("webserver on port %s\n", mg_get_option(server, "listening_port"));

  // Okay to loop like this because mg_poll_server() does a select() with
  // a 100 msec timeout which will idle this thread most of the time.
  while (!stopServer) {
    mg_poll_server(server, 100);
    current_timer = timer_ms();

    if (time_diff(current_timer, last_timer) > 250) {
      last_timer = current_timer;
      mg_iterate_over_connections(server, iterate_callback);
    }

	sched_yield();
  }

  mg_destroy_server(&server);
  return 0;
}

void web_server_start()
{
	ns_start_thread(&web_server, NULL);
}

void web_server_stop()
{
	stopServer = TRUE;
	sched_yield();
}

void web_server_init()
{
	pthread_mutex_init(&a2w_lock, NULL);
	pthread_mutex_init(&w2a_lock, NULL);
}
