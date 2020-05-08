/**
 * cbot_cli.c: CBot backend for command line
 */

#include <stdio.h>
#include <string.h>

#include <sc-argparse.h>

#include "cbot_private.h"
#include "libstephen/str.h"

static void cbot_cli_send(const struct cbot *bot, const char *dest,
                          const char *msg)
{
	(void)bot; // unused
	printf("[%s]%s: %s\n", dest, bot->name, msg);
}

static void cbot_cli_me(const struct cbot *bot, const char *dest,
                        const char *msg)
{
	(void)bot; // unused
	printf("[%s]%s %s\n", dest, bot->name, msg);
}

static void cbot_cli_op(const struct cbot *bot, const char *channel,
                        const char *person)
{
	(void)bot; // unused
	printf("[%s~CMD]%s: /op %s\n", channel, bot->name, person);
}

static void cbot_cli_join(const struct cbot *bot, const char *channel,
                          const char *password)
{
	(void)bot; // unused
	printf("[%s~CMD]%s: /join %s\n", channel, bot->name, channel);
}

static void cbot_cli_nick(const struct cbot *bot, const char *newnick)
{
	(void)bot; // unused
	printf("%s becomes %s\n", bot->name, newnick);
	cbot_set_nick((struct cbot*)bot, newnick);
}

static void help(void)
{
	puts("usage: cbot cli [options] plugins");
	puts("options:");
	puts("  --hash HASH        set the hash chain tip (required)");
	puts("  --name [name]      set the bot's name");
	puts("  --plugin-dir [dir] set the plugin directory");
	puts("  --help             show this help and exit");
	puts("plugins:");
	puts("  list of names of plugins within plugin-dir (don't include "
	     "\".so\").");
	exit(EXIT_FAILURE);
}

enum {
	ARG_HASH = 0,
	ARG_NAME,
	ARG_PLUGIN_DIR,
	ARG_HELP,
};

void run_cbot_cli(int argc, char **argv)
{
	char *line, *plugin_dir, *hash, *name;
	struct cbot *bot;
	struct cbot_backend backend;
	int rv;
	struct sc_arg args[] = {
		SC_ARG_STRING('H', "--hash", "hash chain tip"),
		SC_ARG_DEF_STRING('n', "--name", "cbot", "bot name"),
		SC_ARG_DEF_STRING('p', "--plugin-dir", "bin/release/plugin",
		                  "plugin directory"),
		SC_ARG_COUNT('h', "--help", "help"),
		SC_ARG_END()
	};

	if ((rv = sc_argparse(args, argc, argv)) < 0) {
		fprintf(stderr, "argument parse error\n");
		help();
	}

	if (args[ARG_HELP].val_int)
		help();
	hash = args[ARG_HASH].val_string;
	name = args[ARG_NAME].val_string;
	plugin_dir = args[ARG_PLUGIN_DIR].val_string;

	backend.send = cbot_cli_send;
	backend.me = cbot_cli_me;
	backend.op = cbot_cli_op;
	backend.join = cbot_cli_join;
	backend.nick = cbot_cli_nick;

	bot = cbot_create("cbot", &backend);

	// Set the hash in the bot.
	void *decoded = base64_decode(hash, 20);
	memcpy(bot->hash, decoded, 20);
	free(decoded);

	cbot_load_plugins(bot, plugin_dir, argv, rv);
	while (!feof(stdin)) {
		printf("> ");
		line = read_line(stdin);
		cbot_handle_message(bot, "stdin", "shell", line, false);
		smb_free(line);
	}

	cbot_delete(bot);
}
