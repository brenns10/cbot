/**
 * cbot_cli.c: CBot backend for command line
 */

#include <stdio.h>
#include <string.h>

#include "cbot_private.h"
#include "libstephen/ad.h"
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

void run_cbot_cli(int argc, char **argv)
{
	char *line, *plugin_dir = "bin/release/plugin";
	char *hash = NULL;
	char *name = "cbot";
	struct cbot *bot;
	struct cbot_backend backend;
	smb_ad args;
	arg_data_init(&args);

	process_args(&args, argc, argv);
	if (check_long_flag(&args, "name")) {
		name = get_long_flag_parameter(&args, "name");
	}
	if (check_long_flag(&args, "plugin-dir")) {
		plugin_dir = get_long_flag_parameter(&args, "plugin-dir");
	}
	if (check_long_flag(&args, "hash")) {
		hash = get_long_flag_parameter(&args, "hash");
	}
	if (check_long_flag(&args, "help")) {
		help();
	}
	if (!(name && plugin_dir)) {
		help();
	}
	if (!hash) {
		help();
	}

	backend.send = cbot_cli_send;
	backend.me = cbot_cli_me;
	backend.op = cbot_cli_op;
	backend.join = cbot_cli_join;

	bot = cbot_create("cbot", &backend);

	// Set the hash in the bot.
	void *decoded = base64_decode(hash, 20);
	memcpy(bot->hash, decoded, 20);
	free(decoded);

	cbot_load_plugins(bot, plugin_dir, ll_get_iter(args.bare_strings));
	while (!feof(stdin)) {
		printf("> ");
		line = read_line(stdin);
		cbot_handle_message(bot, "stdin", "shell", line, false);
		smb_free(line);
	}

	arg_data_destroy(&args);
	cbot_delete(bot);
}
