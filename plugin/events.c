/*
 * events.c: warning about events at local venues
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libconfig.h>
#include <sc-collections.h>
#include <sc-lwt.h>
#include <sc-regex.h>

#include "cbot/cbot.h"
#include "cbot/curl.h"

char *CHANNEL;
int WDAY = 3;
int HOUR = 14;
int MIN = 0;

// Chase Center Calendar (Warriors, Valkyries, and other events)
const char *CHASE_CENTER_ICAL =
        "https://www.chasecentercalendar.com/chasecenter.ics";

// Oracle Park Calendar (Giants and other events)
const char *ORACLE_PARK_ICAL =
        "https://www.chasecentercalendar.com/oraclepark.ics";

// Parse iCal time format (YYYYMMDDTHHMMSSZ) to local time
static time_t parse_ical_time(const char *ical_time)
{
	struct tm tm = { 0 };
	char *endptr = strptime(ical_time, "%Y%m%dT%H%M%SZ", &tm);
	if (!endptr || *endptr) {
		return 0; // Parse error
	}
	return timegm(&tm); // Convert UTC to time_t
}

// chasecentercalendar.com includes away games for Warriors and Valkyries. See
// https://github.com/albertyw/chase-center-calendar/pull/41
// We can easily filter them out by looking for the " at " rather than " vs ".
// We could also parse the location field, but I'd rather filter out bad events
// rather than restrict the location field to just a few possible values.
static bool is_away_game(const char *desc)
{
	return (strncmp(desc, "Warriors at ", 12) == 0 ||
	        strncmp(desc, "Valkyries at ", 13) == 0);
}

static int search_ical_events(const char *ical_data, time_t range_start,
                              time_t range_end, struct sc_charbuf *events)
{
	const char *line = ical_data;
	const char *next_line;
	char current_event[512] = { 0 };
	time_t current_start = 0, current_end = 0;
	int found_events = 0;

	while ((next_line = strchr(line, '\n')) != NULL) {
		size_t line_len = next_line - line;
		char line_buf[sizeof(current_event)];
		if (line_len >= sizeof(line_buf) - 1) {
			line = next_line + 1;
			continue;
		}

		strncpy(line_buf, line, line_len);
		line_buf[line_len] = '\0';

		// Remove carriage return if present
		if (line_len > 0 && line_buf[line_len - 1] == '\r')
			line_buf[line_len - 1] = '\0';

		if (strcmp(line_buf, "BEGIN:VEVENT") == 0) {
			current_event[0] = '\0';
			current_start = current_end = 0;
		} else if (strncmp(line_buf, "SUMMARY:", 8) == 0) {
			strncpy(current_event, line_buf + 8,
			        sizeof(current_event) - 1);
			current_event[sizeof(current_event) - 1] = '\0';
		} else if (strncmp(line_buf, "DTSTART:", 8) == 0) {
			current_start = parse_ical_time(line_buf + 8);
		} else if (strncmp(line_buf, "DTEND:", 6) == 0) {
			current_end = parse_ical_time(line_buf + 6);
		} else if (strcmp(line_buf, "END:VEVENT") == 0 &&
		           current_event[0] && range_end >= current_start &&
		           range_start <= current_end &&
		           !is_away_game(current_event)) {
			struct tm local_start, local_end;
			char start_str[16], end_str[16];

			localtime_r(&current_start, &local_start);
			localtime_r(&current_end, &local_end);
			strftime(start_str, sizeof(start_str), "%I:%M%P",
			         &local_start);
			strftime(end_str, sizeof(end_str), "%I:%M%P",
			         &local_end);
			if (found_events > 0)
				sc_cb_concat(events, " Also, ");
			sc_cb_printf(events, "%s at %s--%s.", current_event,
			             start_str[0] == '0' ? start_str + 1
			                                 : start_str,
			             end_str[0] == '0' ? end_str + 1 : end_str);
			found_events++;
		}

		line = next_line + 1;
	}

	return found_events;
}

static int check_chase_center(struct cbot *bot, time_t range_start,
                              time_t range_end, struct sc_charbuf *events)
{
	char *data = cbot_curl_get(bot, "%s", CHASE_CENTER_ICAL);
	if (!data)
		return -1;

	int ret = search_ical_events(data, range_start, range_end, events);
	free(data);
	return ret;
}

// Oracle Park events (Giants and other events)

static int check_oracle_park(struct cbot *bot, time_t range_start,
                             time_t range_end, struct sc_charbuf *events)
{
	char *data = cbot_curl_get(bot, "%s", ORACLE_PARK_ICAL);
	if (!data)
		return -1;

	int ret = search_ical_events(data, range_start, range_end, events);
	free(data);
	return ret;
}

struct arg {
	char *channel;
	struct cbot *bot;
	time_t start, end;
};

static void run_thread(void *varg)
{
	struct arg *arg = varg;
	struct sc_charbuf chase_events, oracle_events, msg;
	bool err = false;

	sc_cb_init(&chase_events, 256);
	sc_cb_init(&oracle_events, 256);
	sc_cb_init(&msg, 512);

	int chase_count = check_chase_center(arg->bot, arg->start, arg->end,
	                                     &chase_events);
	int oracle_count = check_oracle_park(arg->bot, arg->start, arg->end,
	                                     &oracle_events);

	if (chase_count > 0) {
		sc_cb_printf(&msg, "Chase Center events: %s", chase_events.buf);
	}
	if (oracle_count > 0) {
		if (msg.length)
			sc_cb_concat(&msg, " ");
		sc_cb_printf(&msg, "Oracle Park events: %s", oracle_events.buf);
	}
	if (!msg.length) {
		struct tm tm;
		char date_str[16];
		localtime_r(&arg->start, &tm);
		strftime(date_str, sizeof(date_str), "%Y-%m-%d", &tm);
		sc_cb_printf(&msg,
		             "No evening events (5-10 PM) at Chase Center or "
		             "Oracle Park on %s.",
		             date_str);
	}
	if (err)
		sc_cb_printf(&msg,
		             " BTW: I encountered an error in my requests, so"
		             " my result may be inaccurate. Check logs for "
		             "details.");

	cbot_send(arg->bot, arg->channel, "%s", msg.buf);
	sc_cb_destroy(&chase_events);
	sc_cb_destroy(&oracle_events);
	sc_cb_destroy(&msg);
	free(arg->channel);
	free(arg);
}

static time_t next_run(void)
{
	struct tm tm;
	time_t schedule, now = time(NULL);
	localtime_r(&now, &tm);
	tm.tm_isdst = -1; /* reset it for mktime */

	if (tm.tm_wday < WDAY) {
		/* mktime only looks at alterations to mday, not wday or yday.
		 * Add the correct number of days */
		tm.tm_mday += WDAY - tm.tm_wday;
	} else if (tm.tm_wday > WDAY) {
		tm.tm_mday += 7 - tm.tm_wday + WDAY;
	} else if (tm.tm_hour > HOUR ||
	           (tm.tm_hour == HOUR && tm.tm_min >= MIN)) {
		/* If today is the day, we have to skip once it's after
		 * HR_INITIAL */
		tm.tm_mday += 7;
	}

	tm.tm_sec = tm.tm_min = 0;
	tm.tm_hour = HOUR;
	tm.tm_min = MIN;
	tm.tm_sec = 0;
	schedule = mktime(&tm);

	CL_DEBUG("sports_schedule: schedule callback for %s\n", asctime(&tm));
	return schedule;
}

static void callback(struct cbot_plugin *plugin, void *arg)
{
	struct cbot *bot = plugin->bot;
	time_t ts = time(NULL);
	struct tm tm;
	struct arg *a = calloc(1, sizeof(*a));
	a->channel = strdup(CHANNEL);
	a->bot = bot;
	localtime_r(&ts, &tm);
	tm.tm_hour = 17;
	tm.tm_min = 0;
	tm.tm_sec = 0;
	a->start = mktime(&tm);
	tm.tm_hour = 22;
	a->end = mktime(&tm);
	sc_lwt_create_task(cbot_get_lwt_ctx(plugin->bot), run_thread, a);
	plugin->data =
	        cbot_schedule_callback(plugin, &callback, NULL, next_run());
}

static void handler(struct cbot_message_event *evt, void *user)
{
	struct tm tm;
	time_t now = time(NULL);
	localtime_r(&now, &tm);

	char *day = sc_regex_get_capture(evt->message, evt->indices, 2);
	if (!*day || strcmp(day, "today") == 0) {
		// already done above
	} else if (strcmp(day, "tomorrow") == 0) {
		tm.tm_mday++;
	} else {
		strptime(day, "%Y-%m-%d", &tm);
	}
	struct arg *arg = calloc(1, sizeof(*arg));
	arg->channel = strdup(evt->channel);
	arg->bot = evt->bot;
	tm.tm_hour = 17;
	tm.tm_min = 0;
	tm.tm_sec = 0;
	arg->start = mktime(&tm);
	tm.tm_hour = 22;
	arg->end = mktime(&tm);

	sc_lwt_create_task(cbot_get_lwt_ctx(evt->bot), run_thread, arg);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	const char *channel = NULL;
	int rv = config_setting_lookup_string(conf, "channel", &channel);
	if (rv != CONFIG_FALSE) {
		CHANNEL = strdup(channel);
		config_setting_lookup_int(conf, "weekday", &WDAY);
		config_setting_lookup_int(conf, "hour", &HOUR);
		config_setting_lookup_int(conf, "minute", &MIN);
		plugin->data = cbot_schedule_callback(plugin, callback, NULL,
		                                      next_run());
	}
	cbot_register(plugin, CBOT_ADDRESSED, (cbot_handler_t)handler, NULL,
	              "(games|events)( "
	              "(today|tomorrow|\\d\\d\\d\\d-\\d\\d-\\d\\d))?");
	return 0;
}

static void unload(struct cbot_plugin *plugin)
{
	free(CHANNEL);
	if (plugin->data)
		cbot_cancel_callback(plugin->data);
}

static void help(struct cbot_plugin *plugin, struct sc_charbuf *cb)
{
	sc_cb_concat(
	        cb,
	        "- This plugin checks for evening events (5-10 PM) at Chase "
	        "Center & Oracle Park\n"
	        "  Try 'cbot events today' or 'cbot events' for today.\n"
	        "  Try 'cbot events tomorrow' or 'cbot events YYYY-MM-DD' for "
	        "a specific day.\n"
	        "  'cbot games' also works for backward compatibility.");
}

struct cbot_plugin_ops ops = {
	.description = "venue events",
	.load = load,
	.unload = unload,
	.help = help,
};
