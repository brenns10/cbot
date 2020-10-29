/**
 * greet.c: CBot plugin which replies to hello messages
 *
 * Sample usage:
 *
 *     user> hi cbot
 *     cbot> hello, user!
 */

#include <stdlib.h>
#include <string.h>

#include <sc-collections.h>

#include "cbot/cbot.h"

struct cbot_handler *hello_hdlr;

static void cbot_hello(struct cbot_message_event *event, void *user)
{
	cbot_send(event->bot, event->channel, "hello, %s!", event->username);
}

static void register_hello(struct cbot *bot)
{
	struct sc_charbuf buf;
	sc_cb_init(&buf, 256);
	sc_cb_printf(&buf, "[Hh](ello|i|ey),? +%s!?", cbot_get_name(bot));
	hello_hdlr = cbot_register(bot, CBOT_MESSAGE,
	                           (cbot_handler_t)cbot_hello, NULL, buf.buf);
	sc_cb_destroy(&buf);
}

static void cbot_bot_name_change(struct cbot_nick_event *event, void *user)
{
	cbot_deregister(event->bot, hello_hdlr);
	register_hello(event->bot);
}

void greet_load(struct cbot *bot)
{
	register_hello(bot);
	cbot_register(bot, CBOT_BOT_NAME, (cbot_handler_t)cbot_bot_name_change,
	              NULL, NULL);
}
