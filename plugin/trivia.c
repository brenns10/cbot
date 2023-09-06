/**
 * trivia.c: CBot plugin which collects RSVP for trivia
 */
#include <errno.h>
#include <libconfig.h>
#include <sc-collections.h>
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

char *SENDMAIL_COMMAND;
char *CHANNEL;
char *FROM;
char *TO;

struct trivia_reaction {
	char *emoji;
	struct sc_array users;
};

struct trivia_reactions {
	struct sc_array reactions;
	uint64_t handle;
};

static const char *sad_reacts[] = {
	"😥",
	"😢",
	"😭",
};

static void send_trivia_message(struct cbot_plugin *plugin, void *arg);

static void send_rsvp(struct cbot_plugin *plugin, void *arg)
{
	struct trivia_reactions *rxns = arg;
	struct trivia_reaction *arr =
	        sc_arr(&rxns->reactions, struct trivia_reaction);
	int attending = 0, sad = 0;
	struct sc_charbuf msg_attend, msg_sad;
	time_t now, schedule;
	struct tm tm;

	/* Cancel receiving reactions for this message now */
	cbot_unregister_reaction(plugin->bot, rxns->handle);

	/* Schedule the next trivia night callback. It's good to do this before
	 * sending the email since it's not particularly error-prone, so we can
	 * have it done with regardless of the outcome of this email step. */
	now = time(NULL);
	localtime_r(&now, &tm);
	tm.tm_mday += 7;
	tm.tm_hour = HR_INITIAL;
	tm.tm_min = MN_INITIAL;
	tm.tm_sec = 0;
	schedule = mktime(&tm);
	cbot_schedule_callback(plugin, send_trivia_message, NULL, schedule);

	/* Now we can compose the RSVP email */
	sc_cb_init(&msg_attend, 256);
	sc_cb_init(&msg_sad, 256);

	for (size_t react = 0; react < rxns->reactions.len; react++) {
		/* We will write to the normal buffer unless it is a
		 * "sad" emoji according to our authoratative list :P */
		struct sc_charbuf *dst = &msg_attend;
		int *to_increment = &attending;
		int amount = arr[react].users.len;

		for (size_t i = 0; i < nelem(sad_reacts); i++) {
			if (strcmp(arr[react].emoji, sad_reacts[i]) == 0) {
				dst = &msg_sad;
				to_increment = &sad;
				break;
			}
		}

		/* Write the emoji count into the email and track it */
		sc_cb_printf(dst, "%s: %d %s\n", arr[react].emoji, amount,
		             amount > 1 ? "people" : "person");
		*to_increment += amount;

		/* Now free the descriptor, we're done with it */
		char **user_arr = sc_arr(&arr[react].users, char *);
		for (size_t i = 0; i < arr[react].users.len; i++)
			free(user_arr[i]);
		sc_arr_destroy(&arr[react].users);
		free(arr[react].emoji);
	}
	/* We're also done with the reactions array and descriptor */
	sc_arr_destroy(&rxns->reactions);
	free(rxns);

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

	fprintf(f,
	        "From: %s\n"
	        "To: %s\n"
	        "Subject: Trivia Reservation\n\n"
	        "Hi Grace!\n\n"
	        "Today our group should have a total of %d people for "
	        "trivia:\n%s\n"
	        "Can we reserve a table?\n\n"
	        "Thanks,\nStephen's poorly trained bot",
	        FROM, TO, attending, msg_attend.buf);
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
	struct trivia_reactions *rxns = calloc(1, sizeof(*rxns));
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
	tm.tm_hour = HR_SEND_RSVP;
	tm.tm_min = MN_SEND_RSVP;
	tm.tm_sec = 0;
	schedule = mktime(&tm);
	cbot_schedule_callback(plugin, send_rsvp, rxns, schedule);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	int rv;
	time_t schedule, now;
	struct tm tm;
	const char *channel, *sendmail_command, *from, *to;

	trivia_rxn_ops.plugin = plugin;

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
	if (rv == CONFIG_FALSE) {
		fprintf(stderr, "trivia plugin: mising \"from\" config\n");
		return -1;
	}
	rv = config_setting_lookup_string(conf, "to", &to);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr, "trivia plugin: missing \"to\" config\n");
		return -1;
	}
	CHANNEL = strdup(channel);
	SENDMAIL_COMMAND = strdup(sendmail_command);
	FROM = strdup(from);
	TO = strdup(to);

	config_setting_lookup_int(conf, "trivia_weekday", &TRIVIA_WDAY);
	config_setting_lookup_int(conf, "init_hour", &HR_INITIAL);
	config_setting_lookup_int(conf, "init_minute", &MN_INITIAL);
	config_setting_lookup_int(conf, "send_hour", &HR_SEND_RSVP);
	config_setting_lookup_int(conf, "send_minute", &MN_SEND_RSVP);

	now = time(NULL);
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
	cbot_schedule_callback(plugin, send_trivia_message, NULL, schedule);
	return 0;
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
	.help = help,
};
