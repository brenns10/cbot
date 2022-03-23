/**
 * birthday.c: CBot plugin which sends happy birthday messages
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <libconfig.h>

#include "cbot/cbot.h"
#include "cbot/db.h"
#include "sc-collections.h"
#include "sc-lwt.h"
#include "sc-regex.h"

#define LOOP_MIN   5
#define nelem(arr) (sizeof(arr) / sizeof(arr[0]))

const char *tbl_birthday_alters[] = {};

const struct cbot_db_table tbl_birthday = {
	.name = "birthday",
	.version = 0,
	.create = "CREATE TABLE birthday ("
	          "  name TEXT NOT NULL,"
	          "  month INT NOT NULL,"
	          "  day INT NOT NULL"
	          ");",
	.alters = tbl_birthday_alters,
};

struct birthday {
	char *name;
	int month;
	int day;
	struct sc_list_head list;
};

const char *months[] = {
	[1] = "January",  [2] = "February",  [3] = "March",
	[4] = "April",    [5] = "May",       [6] = "June",
	[7] = "July",     [8] = "August",    [9] = "September",
	[10] = "October", [11] = "November", [12] = "December",
};

static int birthday_get_day(struct cbot *bot, int month, int day,
                            struct sc_list_head *res)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, struct birthday,
	                        "SELECT name, month, day FROM birthday "
	                        "WHERE month=$month AND day=$day;");
	CBOTDB_BIND_ARG(int, month);
	CBOTDB_BIND_ARG(int, day);
	CBOTDB_LIST_RESULT(bot, res, CBOTDB_OUTPUT(text, 0, name);
	                   CBOTDB_OUTPUT(int, 1, month);
	                   CBOTDB_OUTPUT(int, 2, day););
}

static int birthday_get_month(struct cbot *bot, int month,
                              struct sc_list_head *res)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, struct birthday,
	                        "SELECT name, month, day FROM birthday "
	                        "WHERE month=$month "
	                        "ORDER BY day ASC;");
	CBOTDB_BIND_ARG(int, month);
	CBOTDB_LIST_RESULT(bot, res, CBOTDB_OUTPUT(text, 0, name);
	                   CBOTDB_OUTPUT(int, 1, month);
	                   CBOTDB_OUTPUT(int, 2, day););
}

static int birthday_get_all(struct cbot *bot, struct sc_list_head *res)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, struct birthday,
	                        "SELECT name, month, day FROM birthday "
	                        "ORDER BY month, day ASC;");
	CBOTDB_LIST_RESULT(bot, res, CBOTDB_OUTPUT(text, 0, name);
	                   CBOTDB_OUTPUT(int, 1, month);
	                   CBOTDB_OUTPUT(int, 2, day););
}

static int birthday_add(struct cbot *bot, char *name, int month, int day)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "INSERT INTO birthday(name, month, day)"
	                        "VALUES($name, $month, $day);");
	CBOTDB_BIND_ARG(text, name);
	CBOTDB_BIND_ARG(int, month);
	CBOTDB_BIND_ARG(int, day);
	CBOTDB_NO_RESULT();
}

static int _birthday_del(struct cbot *bot, char *name)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void,
	                        "DELETE FROM birthday "
	                        "WHERE name=$name;");
	CBOTDB_BIND_ARG(text, name);
	CBOTDB_NO_RESULT();
}

static int _changes(struct cbot *bot)
{
	CBOTDB_QUERY_FUNC_BEGIN(bot, void, "SELECT changes;");
	CBOTDB_SINGLE_INTEGER_RESULT();
}

static int birthday_del(struct cbot *bot, char *name)
{
	_birthday_del(bot, name);
	return _changes(bot);
}

static int birthday_send_day_report(struct cbot *bot, const char *chan,
                                    int month, int day)
{
	struct sc_list_head res;
	struct birthday *b, *n;
	int count = 0;

	sc_list_init(&res);
	birthday_get_day(bot, month, day, &res);
	sc_list_for_each_safe(b, n, &res, list, struct birthday)
	{
		cbot_send(bot, chan, "🎊 Happy Birthday, %s! 🎊", b->name);
		free(b->name);
		free(b);
		count++;
	}
	return count;
}

static int birthday_send_month_report(struct cbot *bot, const char *chan,
                                      int month)
{
	struct sc_list_head res;
	struct birthday *b, *n;
	struct sc_charbuf cb;
	int count = 0;

	sc_list_init(&res);
	sc_cb_init(&cb, 256);
	birthday_get_month(bot, month, &res);
	sc_cb_printf(&cb, "%s birthdays:\n", months[month]);
	sc_list_for_each_safe(b, n, &res, list, struct birthday)
	{
		sc_cb_printf(&cb, "%d/%d: %s\n", b->month, b->day, b->name);
		count++;
		free(b->name);
		free(b);
	}
	if (count)
		cbot_send(bot, chan, "%s", cb.buf);
	sc_cb_destroy(&cb);
	return count;
}

static void cmd_bd_del(struct cbot_message_event *event)
{
	char *name = sc_regex_get_capture(event->message, event->indices, 0);
	int amt = birthday_del(event->bot, name);
	if (amt)
		cbot_send(event->bot, event->channel,
		          "Deleted %d birthday records", amt);
	else
		cbot_send(event->bot, event->channel,
		          "Didn't find any matching records");
}

static void cmd_bd_all(struct cbot_message_event *event)
{
	struct sc_list_head res;
	struct birthday *b, *n;
	int count = 0;
	int permsg = 0;
	struct sc_charbuf cb;

	sc_cb_init(&cb, 1024);
	sc_list_init(&res);
	birthday_get_all(event->bot, &res);
	sc_cb_printf(&cb, "All birthdays\n");
	sc_list_for_each_safe(b, n, &res, list, struct birthday)
	{
		count++;
		permsg++;
		sc_cb_printf(&cb, "%d/%d: %s\n", b->month, b->day, b->name);
		if (permsg >= 5) {
			cbot_send_rl(event->bot, event->channel, "%s", cb.buf);
			sc_cb_clear(&cb);
			permsg = 0;
		}
		free(b->name);
		free(b);
	}
	if (!count)
		cbot_send(event->bot, event->channel,
		          "I have no birthdays recorded");
	else if (permsg)
		cbot_send_rl(event->bot, event->channel, "%s", cb.buf);
	sc_cb_destroy(&cb);
}

static void cmd_bd_add(struct cbot_message_event *event)
{
	char *name, *date;
	int month = 0, day = 0;

	date = sc_regex_get_capture(event->message, event->indices, 0);
	name = sc_regex_get_capture(event->message, event->indices, 1);
	sscanf(date, "%d/%d", &month, &day);

	if (month < 1 || month > 12) {
		cbot_send(event->bot, event->channel, "%d is not a valid month",
		          month);
	} else if (day < 1 || day > 31) {
		cbot_send(event->bot, event->channel, "%d is not a valid day",
		          day);
	} else {
		birthday_add(event->bot, name, month, day);
		cbot_send(event->bot, event->channel,
		          "Ok, I will wish \"%s\" happy birthday on %d/%d",
		          name, month, day);
	}

	free(name);
	free(date);
}

static void cmd_bd_day(struct cbot_message_event *event)
{
	char *date;
	int month = 0, day = 0, count;

	date = sc_regex_get_capture(event->message, event->indices, 0);
	sscanf(date, "%d/%d", &month, &day);
	count = birthday_send_day_report(event->bot, event->channel, month,
	                                 day);
	if (!count)
		cbot_send(event->bot, event->channel,
		          "Sorry, no birthdays on %d/%d", month, day);
}

static void cmd_bd_month(struct cbot_message_event *event)
{
	char *month_str;
	int month, count;

	month_str = sc_regex_get_capture(event->message, event->indices, 1);
	for (month = 1; month < nelem(months); month++) {
		if (strcasecmp(month_str, months[month]) == 0)
			break;
	}
	if (month >= nelem(months)) {
		cbot_send(event->bot, event->channel,
		          "Sorry, I don't know that month");
		return;
	}
	count = birthday_send_month_report(event->bot, event->channel, month);
	if (!count)
		cbot_send(event->bot, event->channel,
		          "Sorry, no birthdays in %s", months[month]);
}

struct bdarg {
	char *channel;
	struct cbot *bot;
	int hour;
	int min;
};

static void bd_thread(void *arg)
{
	struct bdarg *a = arg;
	struct timespec to;
	time_t cur;
	struct tm tm;
	int rep_day, count;

	/* If started end of report loop, don't try to send birthday */
	cur = time(NULL);
	localtime_r(&cur, &tm);
	tm.tm_mon++; /* it's ZERO based? WHY? Days are 1-based! */
	if (tm.tm_hour >= a->hour && tm.tm_min > a->min + LOOP_MIN) {
		CL_DEBUG("birthday: it is %d/%d, time is past report "
		         "threshold, wait for tomorrow\n",
		         tm.tm_mon, tm.tm_mday);
		rep_day = tm.tm_mday;
	} else {
		CL_DEBUG("birthday: it is %d/%d, not yet reached report "
		         "threshold\n",
		         tm.tm_mon, tm.tm_mday);
		rep_day = 0;
	}

	for (;;) {
		to.tv_nsec = 0;
		to.tv_sec = LOOP_MIN * 60;
		sc_lwt_sleep(&to);
		if (sc_lwt_shutting_down())
			break;

		cur = time(NULL);
		localtime_r(&cur, &tm);
		tm.tm_mon++; /* it's ZERO based? WHY? Days are 1-based! */

		/* if it is 9 o'clock of a different day to what we last
		 * reported, go! */
		if (!(tm.tm_mday != rep_day && tm.tm_hour == a->hour &&
		      tm.tm_min >= a->min))
			continue;

		CL_DEBUG("birthday: it is %d/%d, checking birthdays\n",
		         tm.tm_mon, tm.tm_mday);
		count = birthday_send_day_report(a->bot, a->channel, tm.tm_mon,
		                                 tm.tm_mday);
		CL_DEBUG("birthday: sent %d birthday messages\n", count);
		rep_day = tm.tm_mday;

		/* Now, we want to find out if tomorrow is the first of the
		 * month. If so, and if we have birthdays, send a message with
		 * the list of birthdays.
		 */
		cur += 86400;
		localtime_r(&cur, &tm);
		tm.tm_mon++; /* it's ZERO based? WHY? Days are 1-based! */
		if (tm.tm_mday != 1)
			continue;
		CL_DEBUG("birthday: last day of month! send reminder\n");
		count = birthday_send_month_report(a->bot, a->channel,
		                                   tm.tm_mon);
		CL_DEBUG("birthday: reported %d birthdays in month\n", count);
	}

	CL_DEBUG("birthday: exiting thread\n");
	free(a->channel);
	free(a);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	struct bdarg *arg;
	int rv;
	const char *channel;
	int hour = 9;
	int min = 0;

	rv = config_setting_lookup_string(conf, "channel", &channel);
	if (rv == CONFIG_FALSE) {
		fprintf(stderr,
		        "birthday plugin: missing \"channel\" config\n");
		return -1;
	}
	config_setting_lookup_int(conf, "hour", &hour);
	config_setting_lookup_int(conf, "minute", &min);
	arg = calloc(1, sizeof(*arg));
	arg->hour = hour;
	arg->min = min;
	arg->channel = strdup(channel);
	arg->bot = plugin->bot;

	rv = cbot_db_register(plugin, &tbl_birthday);
	if (rv < 0) {
		free(arg->channel);
		free(arg);
		return rv;
	}

	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)cmd_bd_add, NULL,
	              "birthday add ([0-9]+/[0-9]+) (.*)");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)cmd_bd_day, NULL,
	              "birthdays ([0-9]+/[0-9]+)");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)cmd_bd_month,
	              NULL, "birthdays( in)? ([A-Za-z]+)");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)cmd_bd_all, NULL,
	              "birthday list");
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)cmd_bd_del, NULL,
	              "birthday remove (.*)");
	sc_lwt_create_task(cbot_get_lwt_ctx(plugin->bot), bd_thread, arg);

	return 0;
}

struct cbot_plugin_ops ops = {
	.load = load,
};
