/**
 * reply.c: CBot plugin which replies to configured triggers with configured
 * responses
 */
#include <stdlib.h>
#include <string.h>

#include <libconfig.h>
#include <sc-collections.h>
#include <sc-regex.h>

#include "cbot/cbot.h"

struct rep {
	struct sc_list_head list;
	struct cbot_handler *hdlr;
	int count;
	char *replies[0];
};

struct priv {
	struct cbot_plugin *plugin;
	struct sc_list_head replies;
};

static int formatter(struct sc_charbuf *buf, char *key, void *user)
{
	struct cbot_message_event *event = user;
	char *s;
	const char *cs;
	int cap, rv = 0;
	if (strcmp(key, "sender") == 0) {
		sc_cb_concat(buf, event->username);
	} else if (strcmp(key, "channel") == 0) {
		sc_cb_concat(buf, event->channel);
	} else if (strcmp(key, "bot") == 0) {
		cs = cbot_get_name(event->bot);
		sc_cb_concat(buf, cs);
	} else if (sscanf(key, "cap:%d", &cap) == 1) {
		if (cap < 0 || cap >= event->num_captures)
			return -1;
		s = sc_regex_get_capture(event->message, event->indices, 0);
		sc_cb_concat(buf, s);
		free(s);
	} else {
		rv = -1;
	}
	return rv;
}

static void handle_match(struct cbot_message_event *event, void *user)
{
	struct rep *rep = user;
	int response = rand() % rep->count;
	struct sc_charbuf cb;
	int rv;

	sc_cb_init(&cb, 256);
	rv = cbot_format(&cb, rep->replies[response], formatter, event);
	if (rv >= 0)
		cbot_send(event->bot, event->channel, "%s", cb.buf);
	sc_cb_destroy(&cb);
}

static void destroy_replies(struct priv *priv)
{
	struct rep *rep, *next;
	int i;
	sc_list_for_each_safe(rep, next, &priv->replies, list, struct rep)
	{
		cbot_deregister(priv->plugin->bot, rep->hdlr);
		for (i = 0; i < rep->count; i++) {
			free(rep->replies[i]);
		}
		free(rep);
	}
}

static struct rep *add_reply(struct priv *priv, config_setting_t *conf, int idx)
{
	config_setting_t *replies, *el;
	struct rep *rep;
	const char *trigger, *resp;
	int reply_count, rv, i, addressed = false, kind = CBOT_MESSAGE;
	int insensitive = false;
	int flags = 0;

	rv = config_setting_lookup_string(conf, "trigger", &trigger);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr,
		        "plugin.reply.responses[%d].trigger does not "
		        "exist or is not a string\n",
		        idx);
		return NULL;
	}
	rv = config_setting_lookup_bool(conf, "addressed", &addressed);
	if (rv == CONFIG_FALSE)
		addressed = false;
	if (addressed)
		kind = CBOT_ADDRESSED;

	rv = config_setting_lookup_bool(conf, "insensitive", &insensitive);
	if (rv == CONFIG_FALSE)
		insensitive = false;
	if (insensitive)
		flags |= SC_RE_INSENSITIVE;

	rv = config_setting_lookup_string(conf, "response", &resp);
	if (rv == CONFIG_FALSE) {
		replies = config_setting_lookup(conf, "responses");
		if (!replies || !config_setting_is_array(replies)) {
			fprintf(stderr,
			        "plugin.reply.responses[%d] has "
			        "neither string key 'response', or array "
			        "'responses'\n",
			        idx);
			return NULL;
		}
		reply_count = config_setting_length(replies);
		rep = calloc(1, sizeof(*rep) + reply_count * sizeof(char *));
		rep->count = reply_count;
		rep->hdlr = cbot_register2(priv->plugin, kind,
		                           (cbot_handler_t)handle_match, rep,
		                           (char *)trigger, flags);
		for (i = 0; i < reply_count; i++) {
			el = config_setting_get_elem(replies, i);
			resp = config_setting_get_string(el);
			if (!resp) {
				fprintf(stderr,
				        "plugin.reply.responses[%d]"
				        ".responses[%d] is not a string",
				        idx, i);
				free(rep);
				return NULL;
			}
			rep->replies[i] = strdup(resp);
		}
		sc_list_insert_end(&priv->replies, &rep->list);
		return rep;
	} else {
		rep = calloc(1, sizeof(*rep) + 1 * sizeof(char *));
		rep->count = 1;
		rep->replies[0] = strdup(resp);
		rep->hdlr = cbot_register(priv->plugin, kind,
		                          (cbot_handler_t)handle_match, rep,
		                          (char *)trigger);
		sc_list_insert_end(&priv->replies, &rep->list);
		return rep;
	}
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	struct priv *priv = calloc(1, sizeof(struct priv));
	struct config_setting_t *arr, *elem;
	struct rep *rep;
	int i, len;
	priv->plugin = plugin;
	plugin->data = priv;
	sc_list_init(&priv->replies);

	arr = config_setting_lookup(conf, "responses");
	if (!arr || !config_setting_is_list(arr)) {
		fprintf(stderr, "plugins.reply.responses does not exist or is "
		                "not a list\n");
		goto err;
	}

	len = config_setting_length(arr);
	for (i = 0; i < len; i++) {
		elem = config_setting_get_elem(arr, i);
		if (!elem || !config_setting_is_group(elem)) {
			fprintf(stderr,
			        "plugin.reply.responses[%d] is not "
			        "group",
			        i);
			goto cleanup;
		}
		rep = add_reply(priv, elem, i);
		if (!rep)
			goto cleanup;
	}

	return 0;

cleanup:
	destroy_replies(priv);
err:
	free(priv);
	return -1;
}

static void unload(struct cbot_plugin *plugin)
{
	struct priv *priv = plugin->data;
	destroy_replies(priv);
	free(priv);
}

struct cbot_plugin_ops ops = {
	.load = load,
	.unload = unload,
};
