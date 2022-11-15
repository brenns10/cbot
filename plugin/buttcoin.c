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

	bool tether_notified;

	/* The threshold we crossed last time we notified */
	double last_notify_thresh;
};

struct prices {
	double btc;
	double tether;
};

static inline double thresh(double price)
{
	return (double)((int)(price / 1000) * 1000);
}

static const char *URL = "https://pro-api.coinmarketcap.com/v2/cryptocurrency/"
                         "quotes/latest?symbol=BTC,USDT";
static const char *TESTURL = "http://localhost:4100";
static const char *LINK = "https://coinmarketcap.com/currencies/bitcoin/";
static const char *TETHER_LINK = "https://coinmarketcap.com/currencies/tether/";

static int lookup_price(struct buttcoin_notify *butt, struct prices *prices)
{
	struct cbot *bot = butt->bot;
	CURLcode rv;
	int ret;
	CURL *curl = curl_easy_init();
	struct curl_slist *headers = NULL;
	struct sc_charbuf buf;
	struct json_easy *json;
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
		ret = -1;
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
		ret = -1;
		goto out_free_json;
	}
	prices->btc = json_easy_number_get(json, index);

	index = json_easy_lookup(json, 0, "data.USDT[0].quote.USD.price");
	if (index == 0) {
		CL_WARN("buttcoin: quote price not found in response\n");
		ret = -1;
		goto out_free_json;
	}
	prices->tether = json_easy_number_get(json, index);

out_free_json:
	json_easy_free(json);
out:
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	sc_cb_destroy(&buf);
	return ret;
}

static void buttcoin_loop(void *arg)
{
	struct buttcoin_notify *butt = arg;
	double floor;
	struct timespec ts;
	struct prices prices;
	int ret;

	ret = lookup_price(butt, &prices);
	if (ret != 0) {
		CL_WARN("buttcoin: initial price lookup failed, bailing\n");
		return;
	}
	CL_DEBUG("buttcoin: got price: $%.2f\n", prices.btc);
	butt->price_usd = prices.btc;
	/*
	 * If we don't already have the last threshold, simply use the current
	 * price's floor + 1000. So we'll notify when it drops below the current
	 * $1k threshold.
	 */
	if (butt->last_notify_thresh == 0)
		butt->last_notify_thresh = thresh(butt->price_usd) + 1000;

	for (;;) {
		ts.tv_nsec = 0;
		ts.tv_sec = butt->seconds;
		sc_lwt_sleep(&ts);
		if (sc_lwt_shutting_down()) {
			CL_DEBUG("buttcoin: got shutdown signal, goodbye\n");
			break;
		}

		ret = lookup_price(butt, &prices);
		if (ret != 0) {
			CL_WARN("buttcoin: lookup got error, skipping this "
			        "check\n");
			continue;
		}
		floor = thresh(butt->price_usd);
		butt->price_usd = prices.btc;

		if (prices.tether < 0.97 && !butt->tether_notified) {
			cbot_send(butt->bot, butt->channel,
			          "Uh-oh, is Tether losing its peg?\n"
			          "The price is now $%.4f\n"
			          "Live graph: %s",
			          prices.tether, TETHER_LINK);
			butt->tether_notified = true;
		}

		CL_DEBUG("buttcoin: price: $%.2f floor: $%.2f last floor: "
		         "$%.2f\n",
		         prices.btc, floor, butt->last_notify_thresh);
		if (prices.btc >= floor || floor >= butt->last_notify_thresh)
			continue;

		butt->last_notify_thresh = floor;
		cbot_send(butt->bot, butt->channel,
		          "Lol, BTC is now below $%.0f\n"
		          "The price is now $%.2f\n"
		          "Live graph: %s",
		          floor, prices.btc, LINK);
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
	double start_thresh = 0;
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
	config_setting_lookup_float(conf, "btc_thresh_start", &start_thresh);
	config_setting_lookup_bool(conf, "use_test_endpoint", &use_test);
	config_setting_lookup_int(conf, "sleep_interval", &seconds);

	butt = calloc(1, sizeof(*butt));
	butt->bot = plugin->bot;
	butt->channel = strdup(channel);
	sc_cb_init(&cb, 128);
	sc_cb_printf(&cb, "X-CMC_PRO_API_KEY: %s", api_key);
	butt->api_key_header = cb.buf;
	butt->url = use_test ? TESTURL : URL;
	butt->seconds = seconds;
	butt->last_notify_thresh = start_thresh;

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
