/**
 * cbot.c: core CBot implementation
 */

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <libconfig.h>
#include <stdio.h>
#include <string.h>

#include <openssl/evp.h>
#include <sc-collections.h>
#include <sqlite3.h>

#include "cbot_private.h"

struct cbot_backend_ops *all_ops[] = {
	&irc_ops,
	&cli_ops,
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

int cbot_addressed(const struct cbot *bot, const char *message)
{
	int increment = strlen(bot->name);
	if (strncmp(bot->name, message, increment) == 0) {
		while (isspace(message[increment]) ||
		       ispunct(message[increment])) {
			increment++;
		}
		return increment;
	}
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
	OpenSSL_add_all_digests();
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
		        "cbot config: plugin %d missing \"name\" field\n", idx);
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

int cbot_load_config(struct cbot *bot, const char *conf_file)
{
	int rv, i;
	config_t conf;
	config_setting_t *setting, *backgroup, *pluggroup;
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
		fprintf(stderr, "cbot: backend \"%s\" not found\n",
		        bot->backend_name);
		rv = -1;
		goto out;
	}

	backgroup = config_lookup(&conf, bot->backend_name);
	if (!backgroup || !config_setting_is_group(backgroup)) {
		fprintf(stderr, "cbot: \"%s\" section missing or wrong type\n",
		        bot->backend_name);
		rv = -1;
		goto out;
	}
	rv = bot->backend_ops->configure(bot, backgroup);
	if (rv < 0)
		goto out;

	pluggroup = config_lookup(&conf, "plugins");
	if (!pluggroup || !config_setting_is_group(pluggroup)) {
		fprintf(stderr,
		        "cbot: \"plugins\" section missing or wrong type\n");
		rv = -1;
		goto out;
	}
	cbot_load_plugins(bot, pluggroup);

	rv = sqlite3_open_v2("cbot_priv", &bot->privDb,
	                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
	                             SQLITE_OPEN_MEMORY,
	                     NULL);
	if (rv != SQLITE_OK) {
		rv = -1;
		goto out;
	}

	rv = cbot_db_create_tables(bot);
	if (rv < 0) {
		rv = -1;
		goto out;
	}

	bot->lwt_ctx = sc_lwt_init();
	bot->lwt = sc_lwt_create_task(
	        bot->lwt_ctx, (void (*)(void *))bot->backend_ops->run, bot);
out:
	/* only things to cleanup are the config, everything else ought to be
	 * cleaned up by cbot_delete() */
	config_destroy(&conf);
	return rv;
}

void cbot_run(struct cbot *bot)
{
	sc_lwt_run(bot->lwt_ctx);
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
			hdlr->handler((struct cbot_event *)&copy, hdlr->user);
		}
	}

	free((char *)event.old_username);
}
const char *cbot_get_name(struct cbot *bot)
{
	return bot->name;
}

/**
   @brief Free up all resources held by a cbot instance.
   @param cbot The bot to delete.
 */
void cbot_delete(struct cbot *cbot)
{
	struct cbot_handler *hdlr, *next;
	for (int i = 0; i < _CBOT_NUM_EVENT_TYPES_; i++) {
		sc_list_for_each_safe(hdlr, next, &cbot->handlers[i],
		                      handler_list, struct cbot_handler)
		{
			cbot_deregister(cbot, hdlr);
		}
	}
	free_init_channels(cbot);
	if (cbot->privDb) {
		int rv = sqlite3_close(cbot->privDb);
		if (rv != SQLITE_OK) {
			fprintf(stderr,
			        "error closing sqlite db on shutdown\n");
		}
	}
	free(cbot->name);
	free(cbot->backend_name);
	free(cbot->plugin_dir);
	free(cbot->db_file);
	free(cbot);
	EVP_cleanup();
}

/*********
 * Functions related to handlers: registration and handling of events
 *********/

/**
 * Register an event handler for CBot!
 * @param bot The bot instance to register into.
 * @param type The type of event to register a handler for.
 * @param handler Handler to register.
 */
struct cbot_handler *cbot_register(struct cbot *bot, enum cbot_event_type type,
                                   cbot_handler_t handler, void *user,
                                   char *regex)
{
	struct cbot_handler *hdlr = calloc(1, sizeof(*hdlr));
	hdlr->handler = handler;
	hdlr->user = user;
	if (regex)
		hdlr->regex = sc_regex_compile(regex);
	sc_list_insert_end(&bot->handlers[type], &hdlr->handler_list);
	sc_list_init(&hdlr->plugin_list); // TODO add when plugins implemented
	return hdlr;
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
			hdlr->handler((struct cbot_event *)&copy, hdlr->user);
		} else {
			result = sc_regex_exec(hdlr->regex, event.message,
			                       &indices);
			if (result != -1) {
				event.indices = indices;
				event.num_captures =
				        sc_regex_num_captures(hdlr->regex);
				copy = event;
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
                         const char *user, const char *message, bool action)
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
	if (address_increment) {
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
		hdlr->handler((struct cbot_event *)&copy, hdlr->user);
	}
}

/**********
 * Functions related to plugins
 **********/

/**
   @brief Private function to load a single plugin.
 */
static bool cbot_load_plugin(struct cbot *bot, const char *filename,
                             const char *loader)
{
	void *plugin_handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);

	printf("attempting to load function %s from %s\n", loader, filename);

	if (plugin_handle == NULL) {
		fprintf(stderr, "cbot_load_plugin: %s\n", dlerror());
		return false;
	}

	cbot_plugin_t plugin = dlsym(plugin_handle, loader);

	if (plugin == NULL) {
		fprintf(stderr, "cbot_load_plugin: %s\n", dlerror());
		return false;
	}

	plugin(bot);
	return true;
}

/**
   @brief Load a list of plugins from a plugin directory.
 */
int cbot_load_plugins(struct cbot *bot, config_setting_t *group)
{
	struct sc_charbuf name;
	struct sc_charbuf loader;
	const char *plugin_name;
	config_setting_t *entry;
	int i, rv = 0;
	sc_cb_init(&name, 256);
	sc_cb_init(&loader, 256);

	for (i = 0; i < config_setting_length(group); i++) {
		sc_cb_clear(&name);
		sc_cb_clear(&loader);
		entry = config_setting_get_elem(group, i);
		if (!entry || !config_setting_is_group(entry)) {
			fprintf(stderr,
			        "cbot: entry %d in plugins is not a"
			        " group\n",
			        i);
			rv = -1;
			goto out;
		}
		plugin_name = config_setting_name(entry);
		if (!plugin_name) {
			fprintf(stderr, "cbot: plugin entry %d missing name\n",
			        i);
			rv = -1;
			goto out;
		}

		sc_cb_printf(&name, "%s/%s.so", bot->plugin_dir, plugin_name);
		sc_cb_printf(&loader, "%s_load", plugin_name);
		cbot_load_plugin(bot, name.buf, loader.buf);
	}

out:
	sc_cb_destroy(&name);
	sc_cb_destroy(&loader);
	return rv;
}

struct sc_lwt_ctx *cbot_get_lwt_ctx(struct cbot *bot)
{
	return bot->lwt_ctx;
}
