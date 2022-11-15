/*
 * buttcoin.c: CBot plugin which notifies if that stupid fake internet money
 * crashes, good for some laughs
 */
#include <curl/curl.h>
#include <libconfig.h>
#include <sc-collections.h>
#include <sc-lwt.h>
#include <stdlib.h>
#include <time.h>

#include "cbot/cbot.h"
#include "cbot/curl.h"
#include "nosj.h"

struct buttcoin_notify {
	struct cbot *bot;

	/* Coin market cap API key, from config */
	char *api_key_header;
	/* Channel to shitpost in */
	char *channel;
	/* API URL (can sub for test URL) */
	const char *url;
	/* Seconds to sleep between fetches */
	int seconds;
	/* Seconds to wait before renotifying at the same threshold */
	int renotify_wait;

	/* Most recent price */
	double price_usd;

	/* The threshold we crossed last time we notified */
	double last_notify_thresh;

	/* When we last notified */
	struct timeval last_notify_time;
};

static inline double thresh(double price)
{
	return (double)((int)(price / 1000) * 1000);
}

static const char *URL = "https://pro-api.coinmarketcap.com/v2/cryptocurrency/"
                         "quotes/latest?symbol=BTC";
static const char *TESTURL = "http://localhost:4100";
static const char *LINK = "https://coinmarketcap.com/currencies/bitcoin/";

static double lookup_price(struct buttcoin_notify *butt)
{
	struct cbot *bot = butt->bot;
	CURLcode rv;
	int ret;
	CURL *curl = curl_easy_init();
	struct curl_slist *headers = NULL;
	struct sc_charbuf buf;
	struct json_easy *json;
	double price = -1;
	size_t index;

	sc_cb_init(&buf, 256);
	curl_easy_setopt(curl, CURLOPT_URL, butt->url);
	/* Should we null test after each one? Yeah. Will we? No. */
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, butt->api_key_header);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	cbot_curl_charbuf_response(curl, &buf);

	rv = cbot_curl_perform(bot, curl);
	if (rv != CURLE_OK) {
		CL_WARN("buttcoin: curl error: %s\n", curl_easy_strerror(rv));
		goto out;
	}

	json = json_easy_new(buf.buf);
	ret = json_easy_parse(json);
	if (ret != 0) {
		CL_WARN("buttcoin: json parse error: %s\n",
		        json_easy_strerror(ret));
		goto out_free_json;
	}

	index = json_easy_lookup(json, 0, "data.BTC[0].quote.USD.price");
	if (index == 0) {
		CL_WARN("buttcoin: quote price not found in response\n");
		goto out_free_json;
	}
	price = json_easy_number_get(json, index);

out_free_json:
	json_easy_free(json);
out:
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	sc_cb_destroy(&buf);
	return price;
}

static void buttcoin_loop(void *arg)
{
	struct buttcoin_notify *butt = arg;
	double price, floor;
	struct timespec ts;
	struct timeval tv;

	butt->price_usd = lookup_price(butt);
	CL_DEBUG("buttcoin: got price: $%.2f\n", butt->price_usd);
	if (butt->price_usd < 0) {
		CL_WARN("buttcoin: initial price lookup failed, bailing\n");
		return;
	}
	butt->last_notify_thresh = butt->price_usd;

	for (;;) {
		ts.tv_nsec = 0;
		ts.tv_sec = butt->seconds;
		sc_lwt_sleep(&ts);
		if (sc_lwt_shutting_down()) {
			CL_DEBUG("buttcoin: got shutdown signal, goodbye\n");
			break;
		}

		price = lookup_price(butt);
		floor = thresh(butt->price_usd);
		CL_DEBUG("buttcoin: got price: $%.2f, thresh: $%.2f\n", price,
		         floor);
		butt->price_usd = price;
		if (price >= floor)
			continue;

		/*
		 * Ok, price is lower than the 1000 floor of the last price!
		 * But we could be wobbling. Don't notify for the same threshold
		 * within 12 hours.
		 */
		gettimeofday(&tv, NULL);
		if (!(floor < butt->last_notify_thresh ||
		      tv.tv_sec - butt->last_notify_time.tv_sec >=
		              butt->renotify_wait))
			continue;

		butt->last_notify_thresh = floor;
		butt->last_notify_time = tv;
		cbot_send(butt->bot, butt->channel,
		          "Lol, BTC is now below $%.0f\n"
		          "The price is now $%.2f\n"
		          "Live graph: %s",
		          floor, price, LINK);
	}

	/* cleanup butt */
	free(butt->channel);
	free(butt->api_key_header);
	free(butt);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	int rv;
	const char *channel;
	const char *api_key;
	int use_test = 0;
	int seconds = 300;
	int renotify_wait = 12 * 3600;
	struct buttcoin_notify *butt;
	struct sc_charbuf cb;

	rv = config_setting_lookup_string(conf, "channel", &channel);
	if (rv == CONFIG_FALSE) {
		CL_CRIT("buttcoin: missing \"channel\" config\n");
		return -1;
	}
	rv = config_setting_lookup_string(conf, "api_key", &api_key);
	if (rv == CONFIG_FALSE) {
		CL_CRIT("buttcoin: missing \"channel\" config\n");
		return -1;
	}
	config_setting_lookup_bool(conf, "use_test_endpoint", &use_test);
	config_setting_lookup_int(conf, "sleep_interval", &seconds);
	config_setting_lookup_int(conf, "renotify_wait", &renotify_wait);

	butt = calloc(1, sizeof(*butt));
	butt->bot = plugin->bot;
	butt->channel = strdup(channel);
	sc_cb_init(&cb, 128);
	sc_cb_printf(&cb, "X-CMC_PRO_API_KEY: %s", api_key);
	butt->api_key_header = cb.buf;
	butt->url = use_test ? TESTURL : URL;
	butt->seconds = seconds;
	butt->renotify_wait = renotify_wait;

	sc_lwt_create_task(cbot_get_lwt_ctx(plugin->bot), buttcoin_loop, butt);
	return 0;
}

static void help(struct cbot_plugin *plugin, struct sc_charbuf *cb)
{
	sc_cb_concat(cb, "This plugin will send messages when BTC crashes. No "
	                 "commands available.\n");
}

struct cbot_plugin_ops ops = {
	.description = "a plugin to notify you about bitcoin crashing",
	.load = load,
	.help = help,
};
