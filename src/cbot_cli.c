/**
 * cbot_cli.c: CBot backend for command line
 */

#include <libconfig.h>
#include <sc-lwt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(WITH_READLINE)
#include <readline/history.h>
#include <readline/readline.h>
#elif defined(WITH_LIBEDIT)
#include <editline/readline.h>
#endif

#include "cbot/cbot.h"
#include "cbot_private.h"
#include "sc-collections.h"

/****************
 * State held for reactions
 ****************/
struct react_message {
	struct sc_list_head list;
	uint64_t id;
	const struct cbot_reaction_ops *ops;
	void *arg;
};

struct sc_list_head rmsgs;
uint64_t rmsgid = 1;

/***************
 * CBot Backend Callbacks
 ***************/

static uint64_t cbot_cli_send(const struct cbot *bot, const char *dest,
                              const struct cbot_reaction_ops *ops, void *arg,
                              const char *msg)
{
	(void)bot; // unused
	printf("[%s]%s: %s\n", dest, bot->name, msg);
	if (ops) {
		struct react_message *rmsg = calloc(1, sizeof(*rmsg));
		rmsg->id = rmsgid++;
		rmsg->ops = ops;
		rmsg->arg = arg;
		sc_list_init(&rmsg->list);
		sc_list_insert(&rmsgs, &rmsg->list);
		fprintf(stderr, "NOTE: this message has id %lu for reactions\n",
		        rmsg->id);
		return rmsg->id;
	}
	return 0;
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

static void cbot_cli_unregister_reaction(const struct cbot *bot,
                                         uint64_t handle)
{
	struct react_message *msg;
	sc_list_for_each_entry(msg, &rmsgs, list, struct react_message)
	{
		if (msg->id == handle) {
			sc_list_remove(&msg->list);
			free(msg);
			return;
		}
	}
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

static void cbot_cli_cmd_react(struct cbot *bot, int argc, char **argv)
{
	struct react_message *msg;
	uint64_t id = 0;
	struct cbot_reaction_event event = { 0 };
	if (argc != 4) {
		fprintf(stderr, "usage: /react ID USER [-]EMOJI\n");
		return;
	}
	id = strtoull(argv[1], NULL, 10);
	event.bot = bot;
	event.emoji = argv[3];
	event.source = argv[2];
	event.handle = id;
	if (event.emoji[0] == '-') {
		event.emoji = &event.emoji[1];
		event.remove = true;
	}
	sc_list_for_each_entry(msg, &rmsgs, list, struct react_message)
	{
		if (msg->id == id) {
			char *plugname = plugpriv(msg->ops->plugin)->name;
			event.plugin = msg->ops->plugin;
			msg->ops->react_fn(&event, msg->arg);
			fprintf(stderr, "Success! Notified plugin %s\n",
			        plugname);
			return;
		}
	}
	fprintf(stderr, "Failed to react to message with id %luu\n", id);
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
	CMD("/react", cbot_cli_cmd_react, "react to an eligible message"),
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

#if defined(WITH_READLINE) || defined(WITH_LIBEDIT)
static char *cbot_cli_command_compentry(const char *text, int state)
{
	static int index, len;
	if (!state) {
		index = 0;
		len = strlen(text);
	}
	for (; index < nelem(cmds); index++) {
		if (strncmp(cmds[index].cmd, text, len) == 0) {
			return strdup(cmds[index++].cmd);
		}
	}
	return NULL;
}

static char **cbot_cli_completion(const char *text, int start, int end)
{
	/* Tell readline not to try completing filenames */
	rl_attempted_completion_over = 1;

	/* If we're completing the first character of the line, complete command
	 * names */
	if (start == 0)
		return rl_completion_matches(text, cbot_cli_command_compentry);
	else
		return NULL;
}

char *saved_line;
bool line_ready;

static void sc_lwt_readline_cb(char *line)
{
	rl_callback_handler_remove();
	saved_line = line;
	if (line)
		add_history(line);
	line_ready = true;
}

static char *sc_lwt_readline(const char *prompt)
{
	struct sc_lwt *cur = sc_lwt_current();

	saved_line = NULL;
	line_ready = false;
	rl_attempted_completion_function = cbot_cli_completion;
	rl_callback_handler_install(prompt, &sc_lwt_readline_cb);
	while (1) {
		sc_lwt_wait_fd(cur, STDIN_FILENO, SC_LWT_W_IN, NULL);
		sc_lwt_set_state(cur, SC_LWT_BLOCKED);
		sc_lwt_yield();

		int bits = sc_lwt_fd_status(cur, STDIN_FILENO, NULL);
		if (bits & SC_LWT_W_IN) {
			rl_callback_read_char();
			if (line_ready) {
				sc_lwt_remove_fd(cur, STDIN_FILENO);
				return saved_line;
			}
		}
	}
}
#else
static char *sc_lwt_readline(const char *prompt)
{
	char *line = NULL;
	size_t n;
	int rv;
	struct sc_lwt *cur = sc_lwt_current();

	fputs(prompt, stdout);
	fflush(stdout);

	sc_lwt_wait_fd(cur, STDIN_FILENO, SC_LWT_W_IN, NULL);
	sc_lwt_set_state(cur, SC_LWT_BLOCKED);
	sc_lwt_yield();
	sc_lwt_remove_fd(cur, STDIN_FILENO);

	rv = getline(&line, &n, stdin);
	if (rv < 0) {
		if (!feof(stdin))
			perror("getline");
		return NULL;
	}
	return line;
}
#endif

static void cbot_cli_run(struct cbot *bot)
{
	char *line = NULL;
	int newline;
	sc_list_init(&rmsgs);

	while (true) {
		line = sc_lwt_readline("> ");
		if (!line)
			break;
		newline = strlen(line);
		if (newline > 0 && line[newline - 1] == '\n')
			line[newline - 1] = '\0';
		if (line[0] == '/' && cbot_cli_execute_cmd(bot, line))
			continue;
		cbot_handle_message(bot, "stdin", "shell", line, false, false);
		free(line);
	}
}

static int cbot_cli_is_authorized(const struct cbot *bot, const char *user,
                                  const char *msg)
{
	printf("is_authorized(... \"%s\")?\n", user);
	return strcmp(user, "shell") == 0;
}

static int cbot_cli_configure(struct cbot *bot, config_setting_t *group)
{
	return 0;
}

struct cbot_backend_ops cli_ops = {
	.name = "cli",
	.configure = cbot_cli_configure,
	.run = cbot_cli_run,
	.send = cbot_cli_send,
	.me = cbot_cli_me,
	.op = cbot_cli_op,
	.join = cbot_cli_join,
	.nick = cbot_cli_nick,
	.is_authorized = cbot_cli_is_authorized,
	.unregister_reaction = cbot_cli_unregister_reaction,
};
