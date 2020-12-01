/*
 * weather.c: CBot plugin implementing weather queries
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <sc-collections.h>
#include <sc-lwt.h>
#include <sc-regex.h>

#include "cbot/cbot.h"
#include "cbot/curl.h"

char *defloc = "San Francisco";

static ssize_t write_cb(char *data, size_t size, size_t nmemb, void *user)
{
	struct sc_charbuf *buf = user;
	sc_cb_memcpy(buf, data, size * nmemb);
	return size * nmemb;
}

static void sc_cb_rstrip(struct sc_charbuf *buf, char *seq)
{
	int i;
	bool contained;
	while (buf->length > 0) {
		contained = false;
		for (i = 0; seq[i]; i++) {
			if (buf->buf[buf->length - 1] == seq[i]) {
				contained = true;
				break;
			}
		}
		if (!contained)
			return;
		buf->buf[--buf->length] = '\0';
	}
}

struct weather_req {
	struct cbot *bot;
	char *channel;
	char *loc;
	char *urlfmt;
};

static char *mkurl(CURL *easy, const char *format, char *location)
{
	char *encoded = curl_easy_escape(easy, location, 0);
	struct sc_charbuf buf;
	sc_cb_init(&buf, 256);
	sc_cb_printf(&buf, format, encoded);
	curl_free(encoded);
	return buf.buf;
}

static void do_weather(void *data)
{
	struct sc_charbuf buf;
	struct weather_req *req = data;
	struct cbot *bot = req->bot;
	char *url = NULL;
	CURLcode rv;
	sc_cb_init(&buf, 256);
	CURL *easy = curl_easy_init();
	url = mkurl(easy, req->urlfmt, *req->loc ? req->loc : defloc);
	curl_easy_setopt(easy, CURLOPT_URL, url);
	// curl_easy_setopt(easy, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(easy, CURLOPT_WRITEDATA, &buf);
	rv = cbot_curl_perform(bot, easy);
	if (rv != CURLE_OK) {
		fprintf(stderr, "curl: error: %s\n", curl_easy_strerror(rv));
		goto out;
	}
	sc_cb_rstrip(&buf, " \t\r\n");
	cbot_send(bot, req->channel, "%s", buf.buf);
out:
	sc_cb_destroy(&buf);
	free(req->channel);
	free(req->loc);
	free(req);
	free(url);
	curl_easy_cleanup(easy);
}

static void weather(struct cbot_message_event *evt, void *user)
{
	struct weather_req *req = calloc(1, sizeof(*req));
	req->bot = evt->bot;
	req->channel = strdup(evt->channel);
	req->loc = sc_regex_get_capture(evt->message, evt->indices,
	                                evt->num_captures - 1);
	req->urlfmt = (char *)user;
	sc_lwt_create_task(cbot_get_lwt_ctx(evt->bot), do_weather, req);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)weather,
	              (void *)"https://wttr.in/%s?format=4", "weather *(.*)");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)weather,
	              (void *)"https://wttr.in/"
	                      "%s?format=%%l:%%20sunrise:%%S%%20sunset:%%s",
	              "(sunrise|sunset) *(.*)");
	return 0;
}

struct cbot_plugin_ops ops = {
	.load = load,
};
