/*
 * aqi.c: Plugin implementing AQI queries
 */
#include <assert.h>
#include <curl/curl.h>
#include <libconfig.h>
#include <nosj.h>
#include <sc-collections.h>
#include <sc-lwt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbot/cbot.h"
#include "cbot/curl.h"

struct aqi {
	struct cbot_plugin *plugin;
	const char *token;
	int query_count;
};

struct aqi_query {
	struct aqi *aqi;
	char *location;
	char *channel;
};

const char *URLFMT = "https://api.waqi.info/feed/%s/?token=%s";

static inline struct aqi *getaqi(struct cbot_plugin *plugin)
{
	return (struct aqi *)plugin->data;
}

static ssize_t write_cb(char *data, size_t size, size_t nmemb, void *user)
{
	struct sc_charbuf *buf = user;
	sc_cb_memcpy(buf, data, size * nmemb);
	return size * nmemb;
}

static void handle_query(void *data)
{
	struct aqi_query *query = data;
	struct aqi *aqi = query->aqi;
	struct cbot_plugin *plugin = aqi->plugin;
	struct sc_charbuf urlbuf, respbuf;
	CURL *easy;
	struct json_token *tokens;
	struct json_parser p;
	size_t idx;
	int rv, aqival;

	sc_cb_init(&urlbuf, 256);
	sc_cb_init(&respbuf, 512);
	sc_cb_printf(&urlbuf, URLFMT, query->location, aqi->token);

	easy = curl_easy_init();
	curl_easy_setopt(easy, CURLOPT_URL, urlbuf.buf);
	// curl_easy_setopt(easy, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(easy, CURLOPT_WRITEDATA, &respbuf);
	rv = cbot_curl_perform(plugin->bot, easy);
	if (rv != CURLE_OK) {
		fprintf(stderr, "aqi: curl error: %s\n",
		        curl_easy_strerror(rv));
		goto out_curl;
	}
	// printf("RESULT\n%s\nENDRESULT\n", respbuf.buf);

	p = json_parse(respbuf.buf, NULL, -1);
	if (p.error != JSONERR_NO_ERROR) {
		fprintf(stderr, "aqi: nosj error: ");
		json_print_error(stderr, p);
		fprintf(stderr, "\n");
		goto out_curl;
	}
	tokens = calloc(p.tokenidx, sizeof(*tokens));
	p = json_parse(respbuf.buf, tokens, p.tokenidx);
	assert(p.error == JSONERR_NO_ERROR);

	idx = json_lookup(respbuf.buf, tokens, 0, "status");
	if (!idx || !json_string_match(respbuf.buf, tokens, idx, "ok")) {
		char *msg = "(not found)";
		bool freemsg = false;
		idx = json_lookup(respbuf.buf, tokens, 0, "data");
		if (idx && tokens[idx].type == JSON_STRING) {
			msg = malloc(tokens[idx].length + 1);
			json_string_load(respbuf.buf, tokens, idx, msg);
			freemsg = true;
		}
		fprintf(stderr, "aqi: api error: %s\n", msg);
		if (freemsg)
			free(msg);
		goto out_api;
	}

	idx = json_lookup(respbuf.buf, tokens, 0, "data.aqi");
	if (!idx || tokens[idx].type != JSON_NUMBER) {
		fprintf(stderr, "aqi: api didn't return data.aqi\n");
		goto out_api;
	}
	aqival = (int)json_number_get(respbuf.buf, tokens, idx);
	sc_cb_clear(&urlbuf);
	sc_cb_printf(&urlbuf, "AQI: %d", aqival);
	cbot_send(plugin->bot, query->channel, urlbuf.buf);

out_api:
	free(tokens);
out_curl:
	sc_cb_destroy(&urlbuf);
	sc_cb_destroy(&respbuf);
	free(query->channel);
	free(query->location);
	free(query);
	curl_easy_cleanup(easy);
	aqi->query_count--;
}

static void run(struct cbot_message_event *evt, void *user)
{
	struct aqi *aqi = getaqi(evt->plugin);
	struct aqi_query *query = calloc(1, sizeof(*query));
	query->aqi = aqi;
	query->location = strdup("san-francisco");
	query->channel = strdup(evt->channel);
	sc_lwt_create_task(cbot_get_lwt_ctx(evt->bot), handle_query, query);
	aqi->query_count++;
}

static int load(struct cbot_plugin *plugin, config_setting_t *group)
{
	int rv;
	struct aqi *aqi;
	const char *token;

	aqi = calloc(1, sizeof(*aqi));

	rv = config_setting_lookup_string(group, "token", &token);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr, "plugin aqi: missing token!\n");
		rv = -1;
		goto err;
	}
	aqi->token = strdup(token);
	aqi->plugin = plugin;
	plugin->data = aqi;

	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)run, NULL,
	              "aqi *(.*)");

	return 0;
err:
	free(aqi);
	return rv;
}

static void unload(struct cbot_plugin *plugin)
{
	struct aqi *aqi = getaqi(plugin);
	free((void *)aqi->token);
	free(aqi);
	plugin->data = NULL;
}

struct cbot_plugin_ops ops = {
	.load = load,
	.unload = unload,
};
