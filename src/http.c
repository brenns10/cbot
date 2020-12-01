/*
 * Integrating libmicrohttpd with cbot
 */

#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "cbot/cbot.h"
#include "cbot_private.h"

#define PORT 8888

static int method_to_event(const char *method)
{
	if (strcmp(method, "GET") == 0)
		return CBOT_HTTP_GET;
	return -1;
}

static struct cbot_handler *lookup_handler(struct sc_list_head *lh,
                                           size_t **indices, const char *method,
                                           const char *url)
{
	struct cbot_handler *h;
	ssize_t result;
	sc_list_for_each_entry(h, lh, handler_list, struct cbot_handler)
	{
		// No regex matches everything. This probably shouldn't be
		// allowed...
		if (!h->regex)
			return h;
		result = sc_regex_exec(h->regex, url, indices);
		if (result != -1 && url[result] == '\0')
			return h;
	}
	return NULL;
}

static int hdlr(void *cls, struct MHD_Connection *connection, const char *url,
                const char *method, const char *version,
                const char *upload_data, size_t *upload_data_size,
                void **con_cls)
{
	static int aptr;
	const char *notfound = "<html><body><h1>Not Found</h1>"
	                       "<p>CBot ain't got that URL</p></body></html>";
	int evt;
	struct cbot *bot = (struct cbot *)cls;
	struct cbot_handler *h = NULL;
	size_t *indices = NULL;
	struct cbot_http_event event;
	struct MHD_Response *resp;
	enum MHD_Result ret;

	evt = method_to_event(method);

	if (evt != -1)
		h = lookup_handler(&bot->handlers[evt], &indices, method, url);

	if (!h)
		h = lookup_handler(&bot->handlers[CBOT_HTTP_ANY], &indices,
		                   method, url);

	if (!h) {
		/* No registered handler! Return 404. */
		resp = MHD_create_response_from_buffer(strlen(notfound),
		                                       (void *)notfound,
		                                       MHD_RESPMEM_PERSISTENT);
		MHD_add_response_header(resp, "Content-Type",
		                        "text/html; charset=utf-8");
		ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, resp);
		MHD_destroy_response(resp);
		return ret;
	} else if (*con_cls != &aptr) {
		/* We have a registered handler. Continue the connection. */
		*con_cls = &aptr;
		return MHD_YES;
	}

	/* We have a handler, and the request data has arrived. Dispatch the
	 * handler. */
	event.bot = bot;
	event.plugin = &h->plugin->p;
	event.num_captures = 0;
	if (h->regex)
		event.num_captures = sc_regex_num_captures(h->regex);
	event.url = url;
	event.indices = indices;
	event.connection = connection;
	event.method = method;
	event.version = version;
	event.upload_data = upload_data;
	event.upload_data_size = *upload_data_size;
	h->handler((struct cbot_event *)&event, h->user);

	free(indices);
	*con_cls = NULL; /* reset con_cls when done */
	return MHD_YES;  /* TODO: get return value from handler */
}

static void cbot_http_run(void *data)
{
	struct sc_lwt *cur = sc_lwt_current();
	struct cbot *bot = data;
	struct MHD_Daemon *daemon = bot->http;
	fd_set in_fd, out_fd, err_fd;
	struct timespec ts;
	int maxfd, rv;
	unsigned long long req_to;

	while (true) {
		sc_lwt_clear_fds(&in_fd, &out_fd, &err_fd);
		maxfd = 0;
		rv = MHD_get_fdset(daemon, &in_fd, &out_fd, &err_fd, &maxfd);
		if (rv == MHD_NO) {
			fprintf(stderr, "MHD_get_fdset says no\n");
		}
		sc_lwt_add_select_fds(cur, &in_fd, &out_fd, &err_fd, maxfd,
		                      NULL);

		rv = MHD_get_timeout(daemon, &req_to);
		if (rv == MHD_YES) {
			ts.tv_sec = req_to / 1000;
			ts.tv_nsec = req_to * 1000000;
			sc_lwt_settimeout(cur, &ts);
		}

		sc_lwt_set_state(cur, SC_LWT_BLOCKED);
		sc_lwt_yield();

		rv = MHD_run(daemon);
		if (rv == MHD_NO) {
			fprintf(stderr, "MHD_run says no\n");
		}
	}
}

static void cbot_http_root(struct cbot_http_event *evt, void *unused)
{
	const char *me = "<html><body>Hello, browser.</body></html>";
	struct MHD_Response *resp;
	cbot_send(evt->bot, "stdin", "Got request for \"%s\"", evt->url);
	resp = MHD_create_response_from_buffer(strlen(me), (void *)me,
	                                       MHD_RESPMEM_PERSISTENT);
	MHD_add_response_header(resp, "Content-Type",
	                        "text/html; charset=utf-8");
	MHD_queue_response(evt->connection, MHD_HTTP_OK, resp);
	MHD_destroy_response(resp);
}

int cbot_http_init(struct cbot *bot)
{
	bot->http = MHD_start_daemon(MHD_USE_EPOLL, PORT, NULL, NULL,
	                             (MHD_AccessHandlerCallback)hdlr, bot,
	                             MHD_OPTION_END);
	if (!bot->http) {
		return -1;
	}
	bot->lwt = sc_lwt_create_task(bot->lwt_ctx, cbot_http_run, bot);

	cbot_register_priv(bot, NULL, CBOT_HTTP_ANY,
	                   (cbot_handler_t)cbot_http_root, NULL, "/", 0);
	return 0;
}
