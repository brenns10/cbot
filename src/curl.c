/*
 * Thin wrapping over the libcurl multi API.
 */
#include <curl/curl.h>

#include "cbot/cbot.h"
#include "cbot/curl.h"
#include "cbot_private.h"

struct curl_waiting {
	CURL *handle;
	struct sc_lwt *thread;
	CURLcode result;
};

CURLcode cbot_curl_perform(struct cbot *bot, CURL *handle)
{
	struct curl_waiting wait;
	wait.handle = handle;
	wait.thread = sc_lwt_current();
	curl_easy_setopt(handle, CURLOPT_PRIVATE, (char *)&wait);
	curl_multi_add_handle(bot->curlm, handle);
	sc_lwt_set_state(bot->curl_lwt, SC_LWT_RUNNABLE);
	sc_lwt_set_state(wait.thread, SC_LWT_BLOCKED);
	sc_lwt_yield();
	curl_multi_remove_handle(bot->curlm, handle);
	return wait.result;
}

void cbot_curl_run(void *data)
{
	struct cbot *bot = data;
	struct sc_lwt *cur = sc_lwt_current();
	struct curl_waiting *waiting;
	int nhdl;
	fd_set in_fd, out_fd, err_fd;
	int maxfd = 0;
	CURLMcode rv;
	CURLMsg *msg;

	bot->curl_lwt = cur;

	while (true) {
		FD_ZERO(&in_fd);
		FD_ZERO(&out_fd);
		FD_ZERO(&err_fd);
		maxfd = 0;
		curl_multi_fdset(bot->curlm, &in_fd, &out_fd, &err_fd, &maxfd);
		for (int i = 0; i <= maxfd; i++) {
			int flags = 0;
			if (FD_ISSET(i, &in_fd))
				flags |= SC_LWT_W_IN;
			if (FD_ISSET(i, &out_fd))
				flags |= SC_LWT_W_OUT;
			if (FD_ISSET(i, &err_fd))
				flags |= SC_LWT_W_ERR;
			if (flags)
				sc_lwt_wait_fd(cur, i, flags, NULL);
		}
		sc_lwt_set_state(cur, SC_LWT_BLOCKED);
		sc_lwt_yield();

		rv = curl_multi_perform(bot->curlm, &nhdl);
		if (rv != CURLM_OK) {
			printf("curlm error %d: %s\n", rv,
			       curl_multi_strerror(rv));
			/* TODO cleanup */
		}

		do {
			msg = curl_multi_info_read(bot->curlm, &nhdl);
			if (msg && msg->msg == CURLMSG_DONE) {
				curl_easy_getinfo(msg->easy_handle,
				                  CURLINFO_PRIVATE, &waiting);
				waiting->result = msg->data.result;
				sc_lwt_set_state(waiting->thread,
				                 SC_LWT_RUNNABLE);
			}
		} while (msg);

		sc_lwt_remove_all(cur);
	}
}

int cbot_curl_init(struct cbot *bot)
{
	bot->curlm = curl_multi_init();
	bot->curl_lwt = sc_lwt_create_task(bot->lwt_ctx, cbot_curl_run, bot);
	return 0;
}
