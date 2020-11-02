/**
 * cbot_cli.c: CBot backend for command line
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sc-argparse.h>

#include "cbot_private.h"

/***************
 * CBot Backend Callbacks
 ***************/

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
	cbot_set_nick((struct cbot *)bot, newnick);
}

/***************
 * CLI Commands
 ***************/

static void cbot_cli_cmd_add_membership(struct cbot *bot, int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: /memberadd user #channel\n");
		return;
	}
	cbot_add_membership(bot, argv[1], argv[2]);
}

static void cbot_cli_cmd_get_members(struct cbot *bot, int argc, char **argv)
{
	struct cbot_user_info *info = NULL;
	struct sc_list_head head;
	sc_list_init(&head);

	if (argc != 2) {
		fprintf(stderr, "usage: /memberlist #channel\n");
		return;
	}

	cbot_get_members(bot, argv[1], &head);
	sc_list_for_each_entry(info, &head, list, struct cbot_user_info)
	{
		printf("%s\n", info->username);
	}
	cbot_user_info_free_all(&head);
}

static void cbot_cli_cmd_help(struct cbot *bot, int argc, char **argv);

struct cbot_cli_cmd {
	char *cmd;
	int cmdlen;
	void (*func)(struct cbot *, int, char **);
	char *help;
};

#define CMD(cmd, func, help)                                                   \
	{                                                                      \
		cmd, sizeof(cmd) - 1, func, help                               \
	}
const struct cbot_cli_cmd cmds[] = {
	CMD("/memberadd", cbot_cli_cmd_add_membership,
	    "add a member to a cbot channel"),
	CMD("/memberlist", cbot_cli_cmd_get_members,
	    "list members in a cbot channel"),
	CMD("/help", cbot_cli_cmd_help, "list all commands"),
};

static void cbot_cli_cmd_help(struct cbot *bot, int argc, char **argv)
{
	int maxsize = 0;
	for (int i = 0; i < nelem(cmds); i++)
		if (cmds[i].cmdlen > maxsize)
			maxsize = cmds[i].cmdlen;

	maxsize += 1;
	for (int i = 0; i < nelem(cmds); i++) {
		printf("%s:", cmds[i].cmd);
		for (int j = 0; j < maxsize - cmds[i].cmdlen; j++)
			fputc(' ', stdout);
		printf("%s\n", cmds[i].help);
	}
}

#define CBOT_CLI_TOK_BUFSIZE 64
#define CBOT_CLI_TOK_DELIM   " \t\r\n\a"
char **cbot_cli_split_line(char *line, int *count)
{
	int bufsize = CBOT_CLI_TOK_BUFSIZE, position = 0;
	char **tokens = malloc(bufsize * sizeof(char *));
	char *token, **tokens_backup, *save;

	if (!tokens) {
		fprintf(stderr, "cli: allocation error\n");
		exit(EXIT_FAILURE);
	}

	token = strtok_r(line, CBOT_CLI_TOK_DELIM, &save);
	while (token != NULL) {
		tokens[position] = token;
		position++;

		if (position >= bufsize) {
			bufsize += CBOT_CLI_TOK_BUFSIZE;
			tokens_backup = tokens;
			tokens = realloc(tokens, bufsize * sizeof(char *));
			if (!tokens) {
				free(tokens_backup);
				fprintf(stderr, "cli: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}

		token = strtok_r(NULL, CBOT_CLI_TOK_DELIM, &save);
	}
	tokens[position] = NULL;
	if (count)
		*count = position;
	return tokens;
}

bool cbot_cli_execute_cmd(struct cbot *bot, char *line)
{
	int i, argc;
	char **argv;
	for (i = 0; i < nelem(cmds); i++) {
		if (strncmp(cmds[i].cmd, line, cmds[i].cmdlen) == 0) {
			argv = cbot_cli_split_line(line, &argc);
			cmds[i].func(bot, argc, argv);
			free(argv);
			return true;
		}
	}
	return false;
}

static void cbot_cli_run(void *data)
{
	struct cbot *bot = data;
	char *line = NULL;
	size_t n;
	int newline, rv;
	struct sc_lwt *cur = sc_lwt_current();

	while (true) {
		printf("> ");
		fflush(stdout);
		sc_lwt_wait_fd(cur, STDIN_FILENO, SC_LWT_W_IN, NULL);
		sc_lwt_set_state(cur, SC_LWT_BLOCKED);
		sc_lwt_yield();
		sc_lwt_remove_fd(cur, STDIN_FILENO);
		rv = getline(&line, &n, stdin);
		if (rv < 0 && feof(stdin)) {
			break;
		} else if (rv < 0) {
			perror("getline");
			break;
		}
		newline = strlen(line);
		if (newline > 0 && line[newline - 1] == '\n')
			line[newline - 1] = '\0';
		if (line[0] == '/' && cbot_cli_execute_cmd(bot, line))
			continue;
		cbot_handle_message(bot, "stdin", "shell", line, false);
	}
	free(line);
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
	/* noformat */
	ARG_HASH = 0,
	ARG_NAME,
	ARG_PLUGIN_DIR,
	ARG_HELP,
};

void run_cbot_cli(int argc, char **argv)
{
	char *plugin_dir, *hash, *name;
	struct cbot *bot;
	struct cbot_backend backend;
	int rv;
	struct sc_lwt_ctx *c;
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

	bot = cbot_create(name, &backend);

	// Set the hash in the bot.
	void *decoded = base64_decode(hash, 20);
	memcpy(bot->hash, decoded, 20);
	free(decoded);

	cbot_load_plugins(bot, plugin_dir, argv, rv);

	c = sc_lwt_init();
	sc_lwt_create_task(c, cbot_cli_run, bot);
	sc_lwt_run(c);

	cbot_delete(bot);
}
