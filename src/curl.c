/*
 * Thin wrapping over the libcurl multi API.
 */
#include <curl/curl.h>
#include <curl/multi.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>

#include "cbot/cbot.h"
#include "cbot/curl.h"
#include "cbot_private.h"
#include "sc-collections.h"
#include "sc-lwt.h"

static ssize_t write_cb(char *data, size_t size, size_t nmemb, void *user)
{
	struct sc_charbuf *buf = user;
	sc_cb_memcpy(buf, data, size * nmemb);
	return size * nmemb;
}

void cbot_curl_charbuf_response(CURL *easy, struct sc_charbuf *buf)
{
	curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(easy, CURLOPT_WRITEDATA, buf);
}

struct sc_list_head waitlist;

struct curl_waiting {
	struct sc_list_head list;
	CURL *handle;
	struct sc_lwt *thread;
	CURLcode result;
	bool done;
};

CURLcode cbot_curl_perform(struct cbot *bot, CURL *handle)
{
	struct curl_waiting wait;
	bool first = true;
	wait.handle = handle;
	wait.done = false;
	wait.thread = sc_lwt_current();
	sc_list_init(&wait.list);
	curl_easy_setopt(handle, CURLOPT_PRIVATE, (char *)&wait);
	curl_multi_add_handle(bot->curlm, handle);
	sc_lwt_set_state(bot->curl_lwt, SC_LWT_RUNNABLE);
	sc_list_insert_end(&waitlist, &wait.list);
	while (!wait.done) {
		CL_DEBUG("curl: %s request, yielding\n",
		         first ? "enqueued" : "continue");
		first = false;
		/*
		 * The LWT system may wake us up in the case of a shutdown.
		 * The CURL thread will also get woken up, and it will do
		 * cleanup, and then re-wake us up. So, if we're woken up
		 * without the done flag set, we should continue to wait.
		 */
		sc_lwt_set_state(wait.thread, SC_LWT_BLOCKED);
		sc_lwt_yield();
		CL_DEBUG("curl: wakeup, done? %s\n", wait.done ? "yes" : "no");
	}
	return wait.result;
}

void cbot_curl_run(void *data)
{
	struct cbot *bot = data;
	struct sc_lwt *cur = sc_lwt_current();
	struct curl_waiting *waiting, *next;
	struct timespec ts;
	int nhdl, maxfd;
	long millis;
	bool block;
	fd_set in_fd, out_fd, err_fd;
	CURLMcode rv;
	CURLMsg *msg;

	bot->curl_lwt = cur;
	sc_list_init(&waitlist);

	while (true) {
		/*
		 * First, we should mark each file descriptor that curl would
		 * like us to wait on, in case we end up blocking.
		 */
		sc_lwt_fdgen_advance(cur);
		sc_lwt_clear_fds(&in_fd, &out_fd, &err_fd);
		maxfd = 0;
		curl_multi_fdset(bot->curlm, &in_fd, &out_fd, &err_fd, &maxfd);
		sc_lwt_add_select_fds(cur, &in_fd, &out_fd, &err_fd, maxfd,
		                      NULL);
		sc_lwt_fdgen_purge(cur);

		/*
		 * Sometimes, curl doesn't actually want to block, or wants us
		 * to block but do a timeout. We need to ask it how long it
		 * wants to wait.
		 */
		block = true;
		millis = 0;
		curl_multi_timeout(bot->curlm, &millis);
		/* Clear any previously held timeout for our thread */
		sc_lwt_cleartimeout(cur);
		if (millis > 0) {
			ts.tv_sec = millis / 1000;
			ts.tv_nsec = millis * 1000000;
			sc_lwt_settimeout(cur, &ts);
			CL_DEBUG("curlthread: set timeout %d millis\n", millis);
		} else if (millis == 0) {
			block = false;
		}

		/* Only block if curl gave a non-zero timeout */
		if (block) {
			CL_DEBUG("curlthread: yielding\n");
			sc_lwt_set_state(cur, SC_LWT_BLOCKED);
			sc_lwt_yield();
			CL_DEBUG("curlthread: wake up\n");
			if (sc_lwt_shutting_down()) {
				CL_DEBUG("curlthread: shutting down\n");
				break;
			}
		}

		/* Now actually drive curl connections forward. */
		rv = curl_multi_perform(bot->curlm, &nhdl);
		if (rv != CURLM_OK) {
			CL_CRIT("curlm error %d: %s\n", rv,
			        curl_multi_strerror(rv));
			break;
		}

		/*
		 * Finally, we must read messages from CURL about which
		 * connections were completed, etc.
		 */
		do {
			msg = curl_multi_info_read(bot->curlm, &nhdl);
			if (msg && msg->msg == CURLMSG_DONE) {
				curl_easy_getinfo(msg->easy_handle,
				                  CURLINFO_PRIVATE, &waiting);
				curl_multi_remove_handle(bot->curlm,
				                         waiting->handle);
				waiting->result = msg->data.result;
				waiting->done = true;
				sc_list_remove(&waiting->list);
				sc_lwt_set_state(waiting->thread,
				                 SC_LWT_RUNNABLE);
			}
		} while (msg);

		/*
		 * Clear file descriptors for this thread until next time.
		 */
		sc_lwt_remove_all(cur);
	}

	sc_list_for_each_safe(waiting, next, &waitlist, list,
	                      struct curl_waiting)
	{
		CL_DEBUG("curlthread: cancel and remove CURL handle+thread\n");
		sc_list_remove(&waiting->list);
		curl_multi_remove_handle(bot->curlm, waiting->handle);
		waiting->result = CURLE_READ_ERROR;
		waiting->done = true;
		sc_lwt_set_state(waiting->thread, SC_LWT_RUNNABLE);
	}
	curl_multi_cleanup(bot->curlm);
	bot->curlm = NULL;
}

int cbot_curl_init(struct cbot *bot)
{
	bot->curlm = curl_multi_init();
	bot->curl_lwt = sc_lwt_create_task(bot->lwt_ctx, cbot_curl_run, bot);
	return 0;
}
