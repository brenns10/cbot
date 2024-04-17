/**
 * trivia.c: CBot plugin which collects RSVP for trivia
 */
#include <errno.h>
#include <libconfig.h>
#include <sc-collections.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cbot/cbot.h"

#define nelem(x) (sizeof(x) / sizeof(x[0]))

int TRIVIA_WDAY = 3;

int HR_INITIAL = 8;
int MN_INITIAL = 0;

int HR_SEND_RSVP = 14;
int MN_SEND_RSVP = 0;

int EMAIL_FORMAT = 1;

char *SENDMAIL_COMMAND;
char *CHANNEL;
char *FROM;
char *TO;
char *FROMNAME = "Trivia Player";
char *TONAME = "Trivia Master";

enum trivia_state {
	TS_IDLE,
	TS_REACTING,
};

struct trivia_reaction {
	char *emoji;
	struct sc_array users;
};

struct trivia_reactions {
	struct sc_array reactions;
	uint64_t handle;
	enum trivia_state state;
	struct cbot_callback *cb;
};

static const char *sad_reacts[] = {
	"😥",
	"😢",
	"😭",
};

static const char *plus_reacts[] = {
	"1️⃣", "2️⃣", "3️⃣", "4️⃣", "5️⃣", "6️⃣", "7️⃣", "8️⃣", "9️⃣",
};

static const char *maybe_reacts[] = {
	"❓",
	"❔",
};

static void send_trivia_message(struct cbot_plugin *plugin, void *arg);

static time_t next_trivia(void)
{
	struct tm tm;
	time_t schedule, now = time(NULL);
	localtime_r(&now, &tm);
	tm.tm_isdst = -1; /* reset it for mktime */

	if (tm.tm_wday < TRIVIA_WDAY) {
		/* mktime only looks at alterations to mday, not wday or yday.
		 * Add the correct number of days */
		tm.tm_mday += TRIVIA_WDAY - tm.tm_wday;
	} else if (tm.tm_wday > TRIVIA_WDAY) {
		tm.tm_mday += 7 - tm.tm_wday + TRIVIA_WDAY;
	} else if (tm.tm_hour > HR_INITIAL ||
	           (tm.tm_hour == HR_INITIAL && tm.tm_min >= MN_INITIAL)) {
		/* If today is wednesday, we have to skip once it's after
		 * HR_INITIAL */
		tm.tm_mday += 7;
	}

	tm.tm_sec = tm.tm_min = 0;
	tm.tm_hour = HR_INITIAL;
	tm.tm_min = MN_INITIAL;
	tm.tm_sec = 0;
	schedule = mktime(&tm);

	CL_DEBUG("trivia: schedule callback for %lu seconds from now\n",
	         schedule - now);
	return schedule;
}

static void send_rsvp(struct cbot_plugin *plugin, void *arg)
{
	struct trivia_reactions *rxns = plugin->data;
	struct trivia_reaction *arr =
	        sc_arr(&rxns->reactions, struct trivia_reaction);
	int attending = 0, maybe = 0, sad = 0;
	struct sc_charbuf msg_attend, msg_sad;

	/* Cancel receiving reactions for this message now */
	cbot_unregister_reaction(plugin->bot, rxns->handle);

	/* Schedule the next trivia night callback. It's good to do this before
	 * sending the email since it's not particularly error-prone, so we can
	 * have it done with regardless of the outcome of this email step. */
	rxns->cb = cbot_schedule_callback(plugin, send_trivia_message, NULL,
	                                  next_trivia());
	rxns->state = TS_IDLE;

	/* Now we can compose the RSVP email */
	sc_cb_init(&msg_attend, 256);
	sc_cb_init(&msg_sad, 256);

	for (size_t react = 0; react < rxns->reactions.len; react++) {
		/* We will write to the normal buffer unless it is a
		 * "sad" emoji according to our authoratative list :P */
		struct sc_charbuf *dst = &msg_attend;
		int *to_increment = &attending;
		int amount = arr[react].users.len;
		int plus = 0;
		bool is_maybe = false;

		for (size_t i = 0; i < nelem(sad_reacts); i++) {
			if (strcmp(arr[react].emoji, sad_reacts[i]) == 0) {
				dst = &msg_sad;
				to_increment = &sad;
				break;
			}
		}

		for (size_t i = 0; i < nelem(plus_reacts); i++) {
			if (strcmp(arr[react].emoji, plus_reacts[i]) == 0) {
				plus = i + 1;
				break;
			}
		}

		for (size_t i = 0; i < nelem(maybe_reacts); i++) {
			if (strcmp(arr[react].emoji, maybe_reacts[i]) == 0) {
				maybe += amount;
				is_maybe = true;
				break;
			}
		}

		/* Write the emoji count into the email and track it */
		sc_cb_printf(dst, "%s: %d %s%s", arr[react].emoji, amount,
		             amount > 1 ? "people" : "person",
		             is_maybe ? " (maybe)" : "");
		if (plus)
			sc_cb_printf(dst, " (+ %d %s%s)", plus,
			             plus > 1 ? "guests" : "guest",
			             amount > 1 ? " each" : "");
		sc_cb_append(dst, '\n');
		*to_increment += amount + amount * plus;

		/* Now free the descriptor, we're done with it */
		char **user_arr = sc_arr(&arr[react].users, char *);
		for (size_t i = 0; i < arr[react].users.len; i++)
			free(user_arr[i]);
		sc_arr_destroy(&arr[react].users);
		free(arr[react].emoji);
	}
	/* We're also done with the reactions array and descriptor */
	sc_arr_destroy(&rxns->reactions);

	/* If there's nobody attending, don't send the email */
	if (!attending) {
		sc_cb_destroy(&msg_attend);
		sc_cb_destroy(&msg_sad);
		cbot_send(plugin->bot, CHANNEL,
		          "I skipped sending an RSVP since there was no "
		          "interest.");
		return;
	}

	/* Launch MSMTP and begin writing our message into the pipe */
	FILE *f = popen(SENDMAIL_COMMAND, "w");

	if (!f) {
		CL_CRIT("send rsvp: %d %s\n", errno, strerror(errno));
		cbot_send(plugin->bot, CHANNEL,
		          "I'm sorry, I tried to RSVP but failed to run the "
		          "email command.");
		sc_cb_destroy(&msg_attend);
		sc_cb_destroy(&msg_sad);
		return;
	}

	if (EMAIL_FORMAT)
		fprintf(f,
		        "From: %s\n"
		        "To: %s\n"
		        "Subject: Trivia Reservation\n\n",
		        FROM, TO);

	fprintf(f, "Hello %s!\n\n", TONAME);
	if (maybe)
		fprintf(f,
		        "Today our group should have %d - %d people for "
		        "trivia:\n",
		        attending - maybe, attending);
	else
		fprintf(f,
		        "Today our group should have a total of %d people for "
		        "trivia:\n",
		        attending);

	fprintf(f,
	        "%s\n"
	        "Can we reserve a table?\n\n"
	        "Thanks,\n%s's poorly trained bot (but also %s"
	        " if you reply to this message)",
	        msg_attend.buf, FROMNAME, FROMNAME);
	if (sad) {
		fprintf(f,
		        "\n\nPS: We also have %d %s who %s very sad to miss "
		        "trivia today:\n\n%s",
		        sad, sad > 1 ? "people" : "person",
		        sad > 1 ? "are" : "is", msg_sad.buf);
	}
	sc_cb_destroy(&msg_attend);
	sc_cb_destroy(&msg_sad);
	fflush(f);
	int status = pclose(f);
	if (status < 0) {
		CL_CRIT("pclose: %d %s\n", errno, strerror(errno));
		cbot_send(plugin->bot, CHANNEL,
		          "I'm sorry, I tried to RSVP but got an error writing "
		          "the email.");
		return;
	} else if (status) {
		CL_CRIT("pclose: got command status %d\n", status);
		cbot_send(plugin->bot, CHANNEL,
		          "I'm sorry, I tried to RSVP but the email command "
		          "failed.");
		return;
	}
	cbot_send(plugin->bot, CHANNEL,
	          "Ok, I just sent the RSVP, we're all set for trivia!");
}

int react(struct cbot_reaction_event *event, void *arg)
{
	struct trivia_reactions *rxns = (struct trivia_reactions *)arg;
	struct trivia_reaction *arr =
	        sc_arr(&rxns->reactions, struct trivia_reaction);

	/* First, remove the existing reaction by sender */
	for (size_t react = 0; react < rxns->reactions.len; react++) {
		char **user_arr = sc_arr(&arr[react].users, char *);
		for (size_t user = 0; user < arr[react].users.len; user++) {
			if (strcmp(user_arr[user], event->source) == 0) {
				free(user_arr[user]);
				sc_arr_remove(&arr[react].users, char *, user);
				if (arr[react].users.len == 0) {
					sc_arr_destroy(&arr[react].users);
					free(arr[react].emoji);
					sc_arr_remove(&rxns->reactions,
					              struct trivia_reaction,
					              react);
				}
				goto done_removing;
			}
		}
	}
done_removing:
	if (event->remove)
		return 0;

	/* Now, if the reaction is not a remove, we can add a new emoji or user
	 */
	for (size_t react = 0; react < rxns->reactions.len; react++) {
		if (strcmp(arr[react].emoji, event->emoji) == 0) { /* NOLINT */
			char *user = strdup(event->source);
			sc_arr_append(&arr[react].users, char *, user);
			return 0;
		}
	}
	struct trivia_reaction new = { 0 };
	char *user = strdup(event->source);
	new.emoji = strdup(event->emoji);
	sc_arr_init(&new.users, char *, 1);
	sc_arr_append(&new.users, char *, user);
	sc_arr_append(&rxns->reactions, struct trivia_reaction, new);
	return 0;
}

struct cbot_reaction_ops trivia_rxn_ops = {
	.plugin = NULL,
	.react_fn = react,
};

static void send_trivia_message(struct cbot_plugin *plugin, void *arg)
{
	time_t now, schedule;
	struct tm tm;
	struct trivia_reactions *rxns = plugin->data;
	sc_arr_init(&rxns->reactions, struct trivia_reaction, 1);
	rxns->handle =
	        cbot_sendr(plugin->bot, CHANNEL, &trivia_rxn_ops, rxns,
	                   "Hello everyone 👋 it's trivia day! Please RSVP by "
	                   "reacting to this message. Any reaction other than "
	                   "😥, 😢, or 😭 will be recorded as a yes. Our emoji "
	                   "(but not names) will be shared with Grace "
	                   "automatically. I will send the RSVP at 2pm!");
	if (!rxns->handle) {
		CL_CRIT("trivia: could not get reaction handle for initial "
		        "message\n");
		sc_arr_destroy(&rxns->reactions);
		free(rxns);
		return;
	}
	now = time(NULL);
	localtime_r(&now, &tm);
	/* In testing, it is possible to trigger the start of trivia after the
	 * trivia RSVP time / hour. In that case, roll over the RSVP time to the
	 * next day */
	if (tm.tm_hour > HR_SEND_RSVP ||
	    (tm.tm_hour == HR_SEND_RSVP && tm.tm_min > MN_SEND_RSVP))
		tm.tm_mday++;
	tm.tm_hour = HR_SEND_RSVP;
	tm.tm_min = MN_SEND_RSVP;
	tm.tm_sec = 0;
	schedule = mktime(&tm);
	rxns->cb = cbot_schedule_callback(plugin, send_rsvp, rxns, schedule);
	rxns->state = TS_REACTING;
}

static void rsvp_trivia(struct cbot_message_event *event, void *user)
{
	struct cbot_plugin *plugin = event->plugin;
	struct trivia_reactions *rxns = plugin->data;

	if (!cbot_is_authorized(event->bot, event->username, event->message))
		return;

	if (rxns->state != TS_REACTING) {
		cbot_send(plugin->bot, event->channel,
		          "Sorry, wrong state for that!");
		return;
	}
	cbot_cancel_callback(rxns->cb);
	send_rsvp(plugin, NULL);
}

static void start_trivia(struct cbot_message_event *event, void *user)
{
	struct cbot_plugin *plugin = event->plugin;
	struct trivia_reactions *rxns = plugin->data;

	if (!cbot_is_authorized(event->bot, event->username, event->message))
		return;

	if (rxns->state != TS_IDLE) {
		cbot_send(plugin->bot, event->channel,
		          "Sorry, wrong state for that!");
		return;
	}
	cbot_cancel_callback(rxns->cb);
	send_trivia_message(plugin, NULL);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	int rv;
	const char *channel, *sendmail_command, *from, *to;
	struct trivia_reactions *rxns = calloc(1, sizeof(*rxns));

	trivia_rxn_ops.plugin = plugin;
	plugin->data = rxns;

	config_setting_lookup_bool(conf, "email_format", (int *)&EMAIL_FORMAT);

	rv = config_setting_lookup_string(conf, "channel", &channel);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr, "trivia plugin: missing \"channel\" config\n");
		return -1;
	}
	rv = config_setting_lookup_string(conf, "sendmail_command",
	                                  &sendmail_command);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr,
		        "trivia plugin: missing \"sendmail_command\" config\n");
		return -1;
	}
	rv = config_setting_lookup_string(conf, "from", &from);
	if (rv == CONFIG_FALSE && EMAIL_FORMAT) {
		fprintf(stderr, "trivia plugin: mising \"from\" config\n");
		return -1;
	}
	rv = config_setting_lookup_string(conf, "to", &to);
	if (rv == CONFIG_FALSE && EMAIL_FORMAT) {
		fprintf(stderr, "trivia plugin: missing \"to\" config\n");
		return -1;
	}
	CHANNEL = strdup(channel);
	SENDMAIL_COMMAND = strdup(sendmail_command);
	if (EMAIL_FORMAT) {
		FROM = strdup(from);
		TO = strdup(to);
	}

	config_setting_lookup_string(conf, "from_name",
	                             (const char **)&FROMNAME);
	config_setting_lookup_string(conf, "to_name", (const char **)&TONAME);
	FROMNAME = strdup(FROMNAME);
	TONAME = strdup(TONAME);

	config_setting_lookup_int(conf, "trivia_weekday", &TRIVIA_WDAY);
	config_setting_lookup_int(conf, "init_hour", &HR_INITIAL);
	config_setting_lookup_int(conf, "init_minute", &MN_INITIAL);
	config_setting_lookup_int(conf, "send_hour", &HR_SEND_RSVP);
	config_setting_lookup_int(conf, "send_minute", &MN_SEND_RSVP);

	rxns->cb = cbot_schedule_callback(plugin, send_trivia_message, NULL,
	                                  next_trivia());

	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)start_trivia,
	              NULL, "trivia start");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)rsvp_trivia, NULL,
	              "trivia rsvp");
	return 0;
}

static void unload(struct cbot_plugin *plugin)
{
	struct trivia_reactions *rxns = plugin->data;
	/* TODO: free queued reactions */
	free(CHANNEL);
	free(SENDMAIL_COMMAND);
	free(FROM);
	free(TO);
	free(FROMNAME);
	free(TONAME);
	cbot_cancel_callback(rxns->cb);
	free(rxns);
}

static void help(struct cbot_plugin *plugin, struct sc_charbuf *cb)
{
	sc_cb_concat(
	        cb,
	        "- This plugin has no commands, it sends messages and handles\n"
	        "  reactions to those messages.");
}
struct cbot_plugin_ops ops = {
	.description = "a plugin that collects RSVPs for trivia night",
	.load = load,
	.unload = unload,
	.help = help,
};
