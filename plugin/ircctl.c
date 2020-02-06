/**
 * ircctl.c: CBot plugin which allows users to request IRC operations like
 * giving op privilege, or joining a channel
 */
#include <stdlib.h>

#include "sc-regex.h"

#include "cbot/cbot.h"

#define HASH "([A-Za-z0-9+=/]+)"
#define NCMD 2
struct sc_regex *commands[NCMD];
void (*handlers[NCMD])(struct cbot_event, char **);

static void op_handler(struct cbot_event event,
                       char **c)
{
	cbot_op(event.bot, event.channel, c[0]);
}

static void join_handler(struct cbot_event event,
                         char **c)
{
	cbot_join(event.bot, c[0], NULL);
}

static void handler(struct cbot_event event)
{
	size_t *indices = NULL;
	char **captures;
	size_t num_cap;
	int incr = cbot_addressed(event.bot, event.message);

	if (!incr)
		return;
	event.message += incr;

	for (int idx = 0; idx < NCMD; idx++) {
		if (sc_regex_exec(commands[idx], event.message, &indices) ==
		    -1) {
			continue;
		}
		num_cap = sc_regex_num_captures(commands[idx]);
		captures =
		        sc_regex_get_captures(event.message, indices, num_cap);
		if (cbot_is_authorized(event.bot, captures[num_cap - 1])) {
			handlers[idx](event, captures);
		} else {
			cbot_send(
			        event.bot, event.channel,
			        "sorry, you aren't authorized to do that!");
		}
		sc_regex_captures_free(captures, num_cap);
		free(indices);
		return;
	}
}

void ircctl_load(struct cbot *bot)
{
	// commands[0] = recomp("join +(.*) " HASH);
	commands[0] = sc_regex_compile("op +(.*) +" HASH);
	handlers[0] = op_handler;

	commands[1] = sc_regex_compile("join +(.*) +" HASH);
	handlers[1] = join_handler;
	// invite = recomp(" invite +(.*) +(.*) " HASH);
	cbot_register(bot, CBOT_CHANNEL_MSG, handler);
}
