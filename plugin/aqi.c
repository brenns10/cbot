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

static void handle_query(void *data)
{
	struct aqi_query *query = data;
	struct aqi *aqi = query->aqi;
	struct cbot_plugin *plugin = aqi->plugin;
	struct sc_charbuf urlbuf, respbuf;
	CURL *easy;
	struct json_easy *json;
	uint32_t idx;
	int rv, aqival;

	sc_cb_init(&urlbuf, 256);
	sc_cb_init(&respbuf, 512);
	sc_cb_printf(&urlbuf, URLFMT, query->location, aqi->token);

	easy = curl_easy_init();
	curl_easy_setopt(easy, CURLOPT_URL, urlbuf.buf);
	// curl_easy_setopt(easy, CURLOPT_VERBOSE, 1L);
	cbot_curl_charbuf_response(easy, &respbuf);
	rv = cbot_curl_perform(plugin->bot, easy);
	if (rv != CURLE_OK) {
		fprintf(stderr, "aqi: curl error: %s\n",
		        curl_easy_strerror(rv));
		goto out_curl;
	}
	// printf("RESULT\n%s\nENDRESULT\n", respbuf.buf);

	json = json_easy_new(respbuf.buf);
	rv = json_easy_parse(json);
	if (rv != JSON_OK) {
		fprintf(stderr, "aqi: nosj error: %s\n", json_strerror(rv));
		goto out_curl;
	}

	rv = json_easy_lookup(json, 0, "status", &idx);
	bool match;
	if (!rv)
		json_easy_string_match(json, idx, "ok", &match);
	if (rv || !match) {
		char *msg = "(not found)";
		bool freemsg = false;
		rv = json_easy_lookup(json, 0, "data", &idx);
		if (!rv) {
			rv = json_easy_string_get(easy, idx, &msg);
			if (!rv)
				freemsg = true;
		}
		fprintf(stderr, "aqi: api error: %s\n", msg);
		if (freemsg)
			free(msg);
		goto out_api;
	}

	rv = json_easy_lookup(json, 0, "data.aqi", &idx);
	if (rv != JSON_OK || json->tokens[idx].type != JSON_NUMBER) {
		fprintf(stderr, "aqi: api didn't return data.aqi\n");
		goto out_api;
	}
	double val;
	rv = json_easy_number_get(json, idx, &val);
	if (rv != JSON_OK) {
		CL_CRIT("aqi: data.aqi is not a number\n");
		goto out_api;
	}
	aqival = (int)val;
	sc_cb_clear(&urlbuf);
	sc_cb_printf(&urlbuf, "AQI: %d", aqival);
	cbot_send(plugin->bot, query->channel, urlbuf.buf);

out_api:
	json_easy_free(json);
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

static void help(struct cbot_plugin *plugin, struct sc_charbuf *cb)
{
	sc_cb_concat(cb,
	             "- cbot aqi: get air quality index of San Francisco\n");
	sc_cb_concat(
	        cb,
	        "- cbot aqi LOCATION: get AQI for LOCATION (use a Zip code)\n");
}

struct cbot_plugin_ops ops = {
	.description =
	        "show the air quality for a location (default San Francisco)",
	.load = load,
	.unload = unload,
	.help = help,
};
