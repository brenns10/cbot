/**
 * cbot.c: core CBot implementation
 */

#include <ctype.h>
#include <dlfcn.h>
#include <libconfig.h>
#include <openssl/evp.h>
#include <sc-collections.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cbot/cbot.h"
#include "cbot_private.h"
#include "sc-lwt.h"
#include "sc-regex.h"

struct cbot_backend_ops *all_ops[] = {
	&irc_ops,
	&cli_ops,
	&signal_ops,
};

struct cbot_qmsg {
	char *msg;
	char *dest;
	struct sc_list_head list;
};

/********
 * Functions which plugins can call to perform actions. These are generally
 * delegated to the backends.
 ********/

void cbot_send(const struct cbot *cbot, const char *dest, const char *format,
               ...)
{
	va_list va;
	struct sc_charbuf cb;
	va_start(va, format);
	sc_cb_init(&cb, 1024);
	sc_cb_vprintf(&cb, (char *)format, va);
	cbot->backend_ops->send(cbot, dest, cb.buf);
	sc_cb_destroy(&cb);
	va_end(va);
}

static void cbot_sender_thread(void *arg)
{
	struct cbot *bot = arg;
	struct cbot_qmsg *qm, *tmp;
	struct sc_lwt *tsk = sc_lwt_current();
	struct timespec to;

	for (;;) {
		to.tv_nsec = 1000 * 1000 * 200; /* 5 per secnd */
		to.tv_sec = 0;
		/* only queue us up if we have queued messages */
		if (bot->msgq.next != &bot->msgq)
			sc_lwt_settimeout(tsk, &to);
		sc_lwt_set_state(tsk, SC_LWT_BLOCKED);
		sc_lwt_yield();
		if (sc_lwt_shutting_down())
			break;

		qm = NULL;
		sc_list_for_each_entry(tmp, &bot->msgq, list, struct cbot_qmsg)
		{
			qm = tmp;
			break;
		}
		if (!qm)
			continue;
		sc_list_remove(&qm->list);
		/* clang-tidy can't handle this, thinks i freed qm */
		cbot_send(bot, qm->dest, "%s", qm->msg); // NOLINT
		CL_DEBUG("Sent queued message\n");
		free(qm->msg);
		free(qm->dest);
		free(qm);
	}
}

void cbot_send_rl(struct cbot *cbot, const char *dest, const char *format, ...)
{
	va_list va;
	struct sc_charbuf cb;
	struct cbot_qmsg *qm = calloc(1, sizeof(*qm));
	sc_list_init(&qm->list);
	va_start(va, format);
	sc_cb_init(&cb, 1024);
	sc_cb_vprintf(&cb, (char *)format, va);
	va_end(va);
	qm->dest = strdup(dest);
	qm->msg = cb.buf;
	// do not destroy cb!
	sc_list_insert_end(&cbot->msgq, &qm->list);
	sc_lwt_set_state(cbot->msgq_thread, SC_LWT_RUNNABLE);
}

void cbot_me(const struct cbot *cbot, const char *dest, const char *format, ...)
{
	va_list va;
	struct sc_charbuf cb;
	va_start(va, format);
	sc_cb_init(&cb, 1024);
	sc_cb_vprintf(&cb, (char *)format, va);
	cbot->backend_ops->me(cbot, dest, cb.buf);
	sc_cb_destroy(&cb);
	va_end(va);
}

void cbot_op(const struct cbot *cbot, const char *channel, const char *person)
{
	cbot->backend_ops->op(cbot, channel, person);
}

void cbot_join(const struct cbot *cbot, const char *channel,
               const char *password)
{
	cbot->backend_ops->join(cbot, channel, password);
}

void cbot_nick(const struct cbot *cbot, const char *newnick)
{
	cbot->backend_ops->nick(cbot, newnick);
}

static int addressed_advance(const char *message, int i)
{
	while (isspace(message[i]) || ispunct(message[i])) {
		i++;
	}
	return i;
}

int cbot_addressed(const struct cbot *bot, const char *message)
{
	int increment = strlen(bot->name);
	int i;
	if (strncmp(bot->name, message, increment) == 0)
		return addressed_advance(message, increment);
	for (i = 0; i < bot->aliases.len; i++) {
		char *alias = sc_arr(&bot->aliases, char *)[i];
		increment = strlen(alias);
		if (strncmp(alias, message, increment) == 0)
			return addressed_advance(message, increment);
	}
	return 0;
}

void cbot_add_alias(struct cbot *bot, const char *alias)
{
	sc_arr_append(&bot->aliases, char *, strdup(alias));
}

int cbot_is_authorized(struct cbot *bot, const char *user, const char *msg)
{
	if (bot->backend_ops->is_authorized)
		return bot->backend_ops->is_authorized(bot, user, msg);
	return 0;
}

/********
 * Functions related to the bot lifetime
 ********/

/**
   @brief Create a cbot instance.

   A cbot instance is the heart of how cbot works.  In order to create one, you
   need to implement the functions that allow cbot to send on your backend
   (typically IRC, but also console).
   @param name The name of the cbot.
   @return A new cbot instance.
 */
struct cbot *cbot_create(void)
{
	struct cbot *cbot = calloc(1, sizeof(*cbot));
	for (int i = 0; i < _CBOT_NUM_EVENT_TYPES_; i++) {
		sc_list_init(&cbot->handlers[i]);
	}
	sc_list_init(&cbot->init_channels);
	sc_list_init(&cbot->plugins);
	sc_list_init(&cbot->msgq);
	sc_arr_init(&cbot->aliases, 8, sizeof(char *));
	return cbot;
}

static void conf_error(config_t *conf, char *op)
{
	const char *file, *text;
	int line;
	file = config_error_file(conf);
	line = config_error_line(conf);
	text = config_error_text(conf);
	if (file) {
		fprintf(stderr, "%s:%d: %s (cbot config op %s)", file, line,
		        text, op);
	} else {
		fprintf(stderr, "cbot config (%s): %s (line %d)", op, text,
		        line);
	}
}

static char *conf_str_default(config_setting_t *setting, const char *name,
                              char *default_)
{
	const char *str = NULL;
	int rv = config_setting_lookup_string(setting, name, &str);
	if (rv == CONFIG_FALSE)
		str = default_;
	return strdup(str);
}

static struct cbot_channel_conf *
add_init_channel(struct cbot *bot, config_setting_t *elem, int idx)
{
	const char *cc;
	struct cbot_channel_conf *chan = calloc(1, sizeof(*chan));
	int rv = config_setting_lookup_string(elem, "name", &cc);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr,
		        "cbot config: cbot.channels[%d] missing \"name\" "
		        "field\n",
		        idx);
		goto err_name;
	}
	chan->name = strdup(cc);
	rv = config_setting_lookup_string(elem, "pass", &cc);
	if (rv == CONFIG_TRUE) {
		chan->pass = strdup(cc);
	}
	sc_list_insert_end(&bot->init_channels, &chan->list);
	return chan;
err_name:
	free(chan);
	return NULL;
}

static void free_init_channel(struct cbot_channel_conf *c)
{
	free(c->name);
	if (c->pass)
		free(c->pass);
	free(c);
}

static void free_init_channels(struct cbot *bot)
{
	struct cbot_channel_conf *c, *n;
	sc_list_for_each_safe(c, n, &bot->init_channels, list,
	                      struct cbot_channel_conf)
	{
		sc_list_remove(&c->list);
		free_init_channel(c);
	}
}

static int add_channels(struct cbot *bot, config_setting_t *botsec)
{
	int rv, i;
	config_setting_t *chanlist, *elem;
	chanlist = config_setting_lookup(botsec, "channels");
	if (!chanlist || !config_setting_is_list(chanlist)) {
		fprintf(stderr,
		        "cbot: \"cbot.channels\" section missing or wrong "
		        "type\n");
		rv = -1;
		return rv;
		;
	}
	sc_list_init(&bot->init_channels);
	for (i = 0; i < config_setting_length(chanlist); i++) {
		elem = config_setting_get_elem(chanlist, i);
		if (!config_setting_is_group(elem)) {
			fprintf(stderr,
			        "cbot: \"cbot.channels[%d]\" is not a group\n",
			        i);
			goto cleanup_channels;
		}
		add_init_channel(bot, elem, i);
	}
	return 0;
cleanup_channels:
	free_init_channels(bot);
	return -1;
}

int cbot_load_plugins(struct cbot *bot, config_setting_t *group);

static void cbot_run_in_lwt(struct cbot *bot)
{
	struct timespec t;

	bot->msgq_thread = sc_lwt_create_task(cbot_get_lwt_ctx(bot),
	                                      cbot_sender_thread, bot);
	bot->backend_ops->run(bot);
	CL_DEBUG("Sending shutdown signal and waiting...\n");
	sc_lwt_send_shutdown_signal();
	t.tv_sec = 5;
	t.tv_nsec = 0;
	sc_lwt_settimeout(bot->lwt, &t);
	sc_lwt_join();
	if (sc_lwt_task_count() > 1) {
		CL_CRIT("cbot: BUG: tasks running after join\n");
		sc_lwt_early_term();
	}
}

static void cbot_init_logging(struct cbot *bot, config_setting_t *group)
{
	FILE *f;
	char *file = getenv("CBOT_LOG_FILE");
	char *level = getenv("CBOT_LOG_LEVEL");
	int levelno;
	bool free_file = false;
	bool free_level = false;
	if (!file) {
		file = conf_str_default(group, "log_file", ":stderr:");
		free_file = true;
	}
	if (strcmp(file, ":stderr:") == 0)
		cbot_set_log_file(stderr);
	else if ((f = fopen(file, "r")))
		cbot_set_log_file(f);
	else
		perror("cbot: non-fatal error setting up logging");

	if (!level) {
		level = conf_str_default(group, "log_level", "INFO");
		free_level = true;
	}
	levelno = cbot_lookup_level(level);
	cbot_set_log_level(cbot_lookup_level(level));

	printf("cbot: logging to %s at level %d\n", file, levelno);

	if (free_file)
		free(file);
	if (free_level)
		free(level);
}

int cbot_load_config(struct cbot *bot, const char *conf_file)
{
	int rv, i;
	config_t conf;
	config_setting_t *setting, *backgroup, *pluggroup, *httpgroup;
	config_init(&conf);
	rv = config_read_file(&conf, conf_file);
	if (rv == CONFIG_FALSE) {
		conf_error(&conf, "read");
		rv = -1;
		goto out;
	}

	setting = config_lookup(&conf, "cbot");
	if (!setting || !config_setting_is_group(setting)) {
		fprintf(stderr,
		        "cbot: \"cbot\" section missing or wrong type\n");
		rv = -1;
		goto out;
	}

	bot->name = conf_str_default(setting, "name", "cbot");
	bot->backend_name = conf_str_default(setting, "backend", "irc");
	bot->plugin_dir = conf_str_default(setting, "plugin_dir", "build");
	bot->db_file = conf_str_default(setting, "db", "db.sqlite3");
	cbot_init_logging(bot, setting);

	rv = add_channels(bot, setting);
	if (rv < 0) {
		rv = -1;
		goto out;
	}

	for (i = 0; i < nelem(all_ops); i++) {
		if (strcmp(bot->backend_name, all_ops[i]->name) == 0) {
			bot->backend_ops = all_ops[i];
			break;
		}
	}
	if (!bot->backend_ops) {
		CL_CRIT("cbot: backend \"%s\" not found\n", bot->backend_name);
		rv = -1;
		goto out;
	}

	backgroup = config_lookup(&conf, bot->backend_name);
	if (!backgroup || !config_setting_is_group(backgroup)) {
		CL_CRIT("cbot: \"%s\" section missing or wrong type\n",
		        bot->backend_name);
		rv = -1;
		goto out;
	}
	rv = bot->backend_ops->configure(bot, backgroup);
	if (rv < 0)
		goto out;

	bot->lwt_ctx = sc_lwt_init();
	bot->lwt = sc_lwt_create_task(bot->lwt_ctx,
	                              (void (*)(void *))cbot_run_in_lwt, bot);

	rv = cbot_curl_init(bot);
	if (rv < 0)
		goto out;

	rv = cbot_db_init(bot);
	if (rv < 0) {
		rv = -1;
		goto out;
	}

	httpgroup = config_lookup(&conf, "http");
	if (httpgroup && !config_setting_is_group(httpgroup)) {
		CL_CRIT("cbot: \"http\" section should be a group\n");
		rv = -1;
		goto out;
	} else if (httpgroup) {
		rv = cbot_http_init(bot, httpgroup);
		if (rv < 0) {
			rv = -1;
			goto out;
		}
	}

	pluggroup = config_lookup(&conf, "plugins");
	if (!pluggroup || !config_setting_is_group(pluggroup)) {
		CL_CRIT("cbot: \"plugins\" section missing or wrong type\n");
		rv = -1;
		goto out;
	}
	rv = cbot_load_plugins(bot, pluggroup);

out:
	/* only things to cleanup are the config, everything else ought to be
	 * cleaned up by cbot_delete() */
	config_destroy(&conf);
	return rv;
}

void cbot_run(struct cbot *bot)
{
	sc_lwt_run(bot->lwt_ctx);
	CL_DEBUG("exiting run() loop, goodbye!");
}

void cbot_set_nick(struct cbot *bot, const char *newname)
{
	struct cbot_nick_event event, copy;
	struct cbot_handler *hdlr, *next;

	event.bot = bot;
	event.old_username = bot->name;
	bot->name = strdup(newname);
	event.new_username = bot->name;
	event.type = CBOT_BOT_NAME;

	if (strcmp(event.old_username, event.new_username) != 0) {
		sc_list_for_each_safe(hdlr, next, &bot->handlers[event.type],
		                      handler_list, struct cbot_handler)
		{
			copy = event; /* safe in case of modification */
			copy.plugin = &hdlr->plugin->p;
			hdlr->handler((struct cbot_event *)&copy, hdlr->user);
		}
	}

	free((char *)event.old_username);
}
const char *cbot_get_name(struct cbot *bot)
{
	return bot->name;
}

static void cbot_unload_all_plugins(struct cbot *bot);

/**
   @brief Free up all resources held by a cbot instance.
   @param cbot The bot to delete.
 */
void cbot_delete(struct cbot *cbot)
{
	cbot_unload_all_plugins(cbot);
	free_init_channels(cbot);
	if (cbot->privDb) {
		int rv = sqlite3_close(cbot->privDb);
		if (rv != SQLITE_OK) {
			CL_CRIT("error closing sqlite db on shutdown\n");
		}
	}
	free(cbot->name);
	free(cbot->backend_name);
	free(cbot->plugin_dir);
	free(cbot->db_file);
	sc_lwt_free(cbot->lwt_ctx);
	for (int i = 0; i < cbot->aliases.len; i++) {
		free(sc_arr(&cbot->aliases, char *)[i]);
	}
	sc_arr_destroy(&cbot->aliases);
	cbot_http_destroy(cbot);
	free(cbot);
	EVP_cleanup();
}

/*********
 * Functions related to handlers: registration and handling of events
 *********/

struct cbot_handler *cbot_register_priv(struct cbot *bot,
                                        struct cbot_plugpriv *priv,
                                        enum cbot_event_type type,
                                        cbot_handler_t handler, void *user,
                                        char *regex, int re_flags)
{
	struct cbot_handler *hdlr = calloc(1, sizeof(*hdlr));
	hdlr->handler = handler;
	hdlr->user = user;
	if (regex)
		hdlr->regex = sc_regex_compile2(regex, re_flags);
	sc_list_insert_end(&bot->handlers[type], &hdlr->handler_list);
	if (priv)
		sc_list_insert_end(&priv->handlers, &hdlr->plugin_list);
	else
		sc_list_init(&hdlr->plugin_list);
	hdlr->plugin = priv;
	return hdlr;
}

struct cbot_handler *cbot_register2(struct cbot_plugin *plugin,
                                    enum cbot_event_type type,
                                    cbot_handler_t handler, void *user,
                                    char *regex, int re_flags)
{
	struct cbot_plugpriv *priv = plugpriv(plugin);
	return cbot_register_priv(priv->bot, priv, type, handler, user, regex,
	                          re_flags);
}

struct cbot_handler *cbot_register(struct cbot_plugin *plugin,
                                   enum cbot_event_type type,
                                   cbot_handler_t handler, void *user,
                                   char *regex)
{
	return cbot_register2(plugin, type, handler, user, regex, 0);
}

void cbot_deregister(struct cbot *bot, struct cbot_handler *hdlr)
{
	sc_list_remove(&hdlr->handler_list);
	sc_list_remove(&hdlr->plugin_list);
	if (hdlr->regex) {
		sc_regex_free(hdlr->regex);
	}
	free(hdlr);
}

static void cbot_dispatch_msg(struct cbot *bot, struct cbot_message_event event,
                              enum cbot_event_type type)
{
	struct cbot_handler *hdlr;
	struct cbot_message_event copy;
	size_t *indices;
	int result;
	sc_list_for_each_entry(hdlr, &bot->handlers[type], handler_list,
	                       struct cbot_handler)
	{
		if (!hdlr->regex) {
			event.indices = NULL;
			event.num_captures = 0;
			copy = event; /* safe in case of modification */
			copy.plugin = &hdlr->plugin->p;
			hdlr->handler((struct cbot_event *)&copy, hdlr->user);
		} else {
			result = sc_regex_exec(hdlr->regex, event.message,
			                       &indices);
			if (result != -1 && event.message[result] == '\0') {
				event.indices = indices;
				event.num_captures =
				        sc_regex_num_captures(hdlr->regex);
				copy = event;
				copy.plugin = &hdlr->plugin->p;
				hdlr->handler((struct cbot_event *)&copy,
				              hdlr->user);
				free(indices);
			}
		}
	}
}

/**
 * @brief Function called by backends to handle standard messages
 *
 * CBot has the concept of messages directed AT the bot versus messages said in
 * a channel, but not specifically targeted at the bot. IRC doesn't have this
 * separation, so we have to separate out the two cases and then call the
 * appropriate handlers.
 *
 * IRC also has tho concept of "action" messages, e.g. /me says hello. This
 * function serves these messages too, using the "action" flag.
 *
 * @param bot The bot we're operating on
 * @param channel The channel this message came in
 * @param user The user who said it
 * @param message The message itself
 * @param action True if the message was a CTCP action, false otherwise.
 */
void cbot_handle_message(struct cbot *bot, const char *channel,
                         const char *user, const char *message, bool action,
                         bool is_dm)
{
	struct cbot_message_event event;
	int address_increment = cbot_addressed(bot, message);

	/* shared fields */
	event.bot = bot;
	event.channel = channel;
	event.username = user;
	event.is_action = action;
	event.indices = NULL;

	/* When cbot is directly addressed */
	if (address_increment || is_dm) {
		event.message = message + address_increment;
		event.type = CBOT_ADDRESSED;
		cbot_dispatch_msg(bot, event, CBOT_ADDRESSED);
	}

	event.type = CBOT_MESSAGE;
	event.message = message;
	cbot_dispatch_msg(bot, event, CBOT_MESSAGE);
}

void cbot_handle_user_event(struct cbot *bot, const char *channel,
                            const char *user, enum cbot_event_type type)
{
	struct cbot_user_event event, copy;
	struct cbot_handler *hdlr;
	event.bot = bot;
	event.type = type;
	event.channel = channel;
	event.username = user;

	sc_list_for_each_entry(hdlr, &bot->handlers[type], handler_list,
	                       struct cbot_handler)
	{
		copy = event; /* safe in case of modification */
		copy.plugin = &hdlr->plugin->p;
		hdlr->handler((struct cbot_event *)&copy, hdlr->user);
	}
}

void cbot_handle_nick_event(struct cbot *bot, const char *old_username,
                            const char *new_username)
{
	struct cbot_nick_event event, copy;
	struct cbot_handler *hdlr;
	event.bot = bot;
	event.type = CBOT_NICK;
	event.old_username = old_username;
	event.new_username = new_username;

	sc_list_for_each_entry(hdlr, &bot->handlers[CBOT_NICK], handler_list,
	                       struct cbot_handler)
	{
		copy = event; /* safe in case of modification */
		copy.plugin = &hdlr->plugin->p;
		hdlr->handler((struct cbot_event *)&copy, hdlr->user);
	}
}

/**********
 * Functions related to plugins
 **********/

/**
   @brief Private function to load a single plugin.
 */
static struct cbot_plugpriv *cbot_load_plugin(struct cbot *bot,
                                              const char *filename,
                                              const char *name,
                                              config_setting_t *conf)
{
	void *plugin_handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
	struct cbot_plugin_ops *ops;
	struct cbot_plugpriv *priv;
	int rv;

	CL_INFO("load plugin %s\n", filename);

	if (plugin_handle == NULL) {
		CL_CRIT("cbot_load_plugin: %s\n", dlerror());
		return NULL;
	}

	ops = dlsym(plugin_handle, "ops");

	if (ops == NULL) {
		CL_CRIT("cbot_load_plugin: %s\n", dlerror());
		return NULL;
	}
	priv = calloc(1, sizeof(*priv));
	priv->p.ops = ops;
	priv->p.bot = bot;
	priv->bot = bot;
	priv->name = strdup(name);
	priv->handle = plugin_handle;
	sc_list_init(&priv->list);
	sc_list_init(&priv->handlers);
	rv = ops->load(&priv->p, conf);
	if (rv < 0) {
		free(priv->name);
		dlclose(plugin_handle);
		free(priv);
		CL_CRIT("loader failed with code %d\n", rv);
		return NULL;
	}
	sc_list_insert_end(&bot->plugins, &priv->list);
	return priv;
}

/**
   @brief Load a list of plugins from a plugin directory.
 */
int cbot_load_plugins(struct cbot *bot, config_setting_t *group)
{
	struct sc_charbuf name;
	const char *plugin_name;
	config_setting_t *entry;
	struct cbot_plugpriv *priv;
	int i, rv = 0;
	sc_cb_init(&name, 256);

	for (i = 0; i < config_setting_length(group); i++) {
		sc_cb_clear(&name);
		entry = config_setting_get_elem(group, i);
		if (!entry || !config_setting_is_group(entry)) {
			CL_CRIT("cbot: entry %d in plugins is not a"
			        " group\n",
			        i);
			rv = -1;
			goto out;
		}
		plugin_name = config_setting_name(entry);
		if (!plugin_name) {
			CL_CRIT("cbot: plugin entry %d missing name\n", i);
			rv = -1;
			goto out;
		}

		sc_cb_printf(&name, "%s/%s.so", bot->plugin_dir, plugin_name);
		priv = cbot_load_plugin(bot, name.buf, plugin_name, entry);
		if (!priv) {
			rv = -1;
			goto out;
		}
	}

out:
	sc_cb_destroy(&name);
	return rv;
}

void cbot_unload_plugin(struct cbot_plugin *plugin)
{
	struct cbot_plugpriv *priv = plugpriv(plugin);
	struct cbot_handler *hdlr, *n;
	if (plugin->ops->unload)
		plugin->ops->unload(plugin);
	sc_list_for_each_safe(hdlr, n, &priv->handlers, plugin_list,
	                      struct cbot_handler)
	{
		cbot_deregister(priv->bot, hdlr);
	}
	dlclose(priv->handle);
	free(priv->name);
	free(priv);
}

static void cbot_unload_all_plugins(struct cbot *bot)
{
	struct cbot_plugpriv *priv, *n;
	sc_list_for_each_safe(priv, n, &bot->plugins, list,
	                      struct cbot_plugpriv)
	{
		cbot_unload_plugin(&priv->p);
	}
}

struct sc_lwt_ctx *cbot_get_lwt_ctx(struct cbot *bot)
{
	return bot->lwt_ctx;
}
