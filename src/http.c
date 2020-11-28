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

static int hdlr(void *cls, struct MHD_Connection *connection, const char *url,
                const char *method, const char *version,
                const char *upload_data, size_t *upload_data_size,
                void **con_cls)
{
	static int aptr;
	const char *me = "<html><body>Hello browser</body></html>";
	struct MHD_Response *response;
	enum MHD_Result ret;

	if (0 != strcmp(method, "GET"))
		return MHD_NO; /* unexpected method */
	if (&aptr != *con_cls) {
		/* do never respond on first call */
		*con_cls = &aptr;
		return MHD_YES;
	}
	*con_cls = NULL; /* reset when done */
	response = MHD_create_response_from_buffer(strlen(me), (void *)me,
	                                           MHD_RESPMEM_PERSISTENT);
	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	return ret;
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

int cbot_http_init(struct cbot *bot)
{
	bot->http = MHD_start_daemon(MHD_USE_EPOLL, PORT, NULL, NULL, hdlr, bot,
	                             MHD_OPTION_END);
	if (!bot->http) {
		return -1;
	}
	bot->lwt = sc_lwt_create_task(bot->lwt_ctx, cbot_http_run, bot);
	return 0;
}
