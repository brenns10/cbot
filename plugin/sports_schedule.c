/*
 * sports_schedule.c: warning about sports schedules
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libconfig.h>
#include <nosj.h>
#include <sc-collections.h>
#include <sc-lwt.h>
#include <sc-regex.h>

#include "cbot/cbot.h"
#include "cbot/curl.h"
#include "cbot/json.h"

char *CHANNEL;
int WDAY = 3;
int HOUR = 14;
int MIN = 0;

// NBA Data References
// https://github.com/swar/nba_api/issues/203
const char *NBA_SCHED =
        ("https://stats.nba.com/stats/internationalbroadcasterschedule?"
         "LeagueID=00&Season=%d&RegionID=1&Date=%02d/%02d/%d&EST=Y");
const char *NBA_TEAM = "GSW";
const char *NBA_TEAMNAME = "Golden State Warriors";

static int search_nba_games(struct json_easy *je, int year, int month, int day,
                            char **time)
{
	uint32_t obj;
	char date[12];
	snprintf(date, sizeof(date), "%02d/%02d/%04d", month, day, year);

	if (je_get_array(je, 0, "resultSets", &obj) != JSON_OK) {
		CL_WARN("nba: no 'resultSets' key found");
		return -1; /* not found */
	}
	CL_DEBUG("resultSets = %u\n", obj);

	je_arr_for_each(obj, je, obj)
	{
		CL_DEBUG("  element = %u\n", obj);
		uint32_t k, v;
		je_obj_for_each(k, v, je, obj)
		{
			CL_DEBUG("    object = %u, %u\n", k, v);
			bool match;
			json_easy_string_match(je, k, "NextGameList", &match);
			if (!match)
				json_easy_string_match(
				        je, k, "CompleteGameList", &match);
			if (!match)
				continue;

			uint32_t game;
			je_arr_for_each(game, je, v)
			{
				CL_DEBUG("      CANDIDATE %lu\n", game);
				if (!je_string_match(je, game, "date", date))
					continue;
				if (!je_string_match(je, game, "htAbbreviation",
				                     NBA_TEAM))
					continue;

				CL_DEBUG("      FOUND %lu\n", game);
				char *timestr = NULL;
				if (je_get_string(je, game, "time", &timestr) !=
				            JSON_OK ||
				    !*timestr) {
					free(timestr);
					timestr = strdup("unknown time");
				}
				*time = timestr;
				return 1;
			}
		}
	}
	return 0;
}

static int check_nba(struct cbot *bot, int year, int month, int day,
                     char **time)
{
	/* NBA season overlaps, use the earlier year. Latest the finals could be
	 * is July */
	int season = year;
	if (month <= 7)
		season--;
	char *data = cbot_curl_get(bot, NBA_SCHED, season, month, day, year);
	if (!data)
		return -1;

	struct json_easy je;
	json_easy_init(&je, data);
	int ret = json_easy_parse(&je);
	if (ret != JSON_OK) {
		CL_WARN("nba/json: error: %s\n", json_strerror(ret));
		ret = -1;
	} else {
		ret = search_nba_games(&je, year, month, day, time);
	}
	json_easy_destroy(&je);
	free(data);
	return ret;
}

// MLB Data References
// https://github.com/brianhaferkamp/mlbapidata
const char *MLB_SCHED =
        ("http://statsapi.mlb.com/api/v1/schedule/games/?sportId=1&"
         "startDate=%d-%02d-%02d&endDate=%d-%02d-%02d");
const char *MLB_TEAM = "San Francisco Giants";

static int search_mlb_games(struct json_easy *je, int year, int month, int day,
                            char **time)
{
	uint32_t dates;
	char date[12];
	snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);

	// json_easy_format(je, 0, stdout);

	int ret = je_get_array(je, 0, "dates", &dates);
	if (ret != JSON_OK) {
		CL_WARN("mlb: 'dates' not found in result\n");
		return -1;
	}

	uint32_t obj;
	je_arr_for_each(obj, je, dates)
	{
		if (!je_string_match(je, obj, "date", date)) {
			CL_DEBUG("mlb: skip non-matching date (%s)\n", date);
			continue;
		}
		uint32_t games, game;
		if (je_get_array(je, obj, "games", &games) != JSON_OK) {
			CL_WARN("mlb: missing 'games' in date result\n");
			continue;
		}
		je_arr_for_each(game, je, games)
		{
			if (!je_string_match(je, game, "teams.home.team.name",
			                     MLB_TEAM))
				continue;
			char *timestamp;
			if (je_get_string(je, game, "gameDate", &timestamp) !=
			    JSON_OK) {
				CL_WARN("mlb: could not load 'gameDate'\n");
				continue;
			}
			/*
			 * Time is provided in a ISO-8601 string, so we should
			 * be able to parse it. The resulting "struct tm"
			 * represents broken-down time for the time zone in the
			 * string (seems to be UTC). We need to convert to a
			 * time_t and then use localtime_r() to get a
			 * broken-down time for our time zone, which we can then
			 * present to the user.
			 * EG: "gameDate": "2024-04-17T16:10:00Z"
			 */
			struct tm broken = { 0 };
			char *ret = strptime(timestamp, "%Y-%m-%dT%H:%M:%S%z",
			                     &broken);
			free(timestamp);
			if (!ret || *ret) {
				CL_WARN("mlb: failed to parse 'gameDate'\n");
				continue;
			}
			time_t epoch = timegm(&broken);
			localtime_r(&epoch, &broken);
			char *timeval = malloc(12);
			strftime(timeval, 12, "%I:%M %p", &broken);
			*time = timeval;
			return 1;
		}
	}
	return 0;
}

static int check_mlb(struct cbot *bot, int year, int month, int day,
                     char **time)
{
	char *data = cbot_curl_get(bot, MLB_SCHED, year, month, day, year,
	                           month, day);
	if (!data)
		return -1;

	struct json_easy je;
	json_easy_init(&je, data);
	int ret = json_easy_parse(&je);
	if (ret != JSON_OK) {
		CL_WARN("mlb/json: error: %s\n", json_strerror(ret));
		ret = -1;
	} else {
		ret = search_mlb_games(&je, year, month, day, time);
	}
	json_easy_destroy(&je);
	free(data);
	return ret;
}

struct arg {
	char *channel;
	struct cbot *bot;
	struct tm tm;
	bool report_empty;
	bool reschedule;
};

static void run_thread(void *varg)
{
	struct arg *arg = varg;
	char *mlb_time, *nba_time;
	struct sc_charbuf msg;

	sc_cb_init(&msg, 256);
	int nba = check_nba(arg->bot, arg->tm.tm_year, arg->tm.tm_mon,
	                    arg->tm.tm_mday, &nba_time);
	if (nba > 0) {
		sc_cb_printf(&msg, "The %s have a home game at %s.",
		             NBA_TEAMNAME, nba_time);
		free(nba_time);
	}
	int mlb = check_mlb(arg->bot, arg->tm.tm_year, arg->tm.tm_mon,
	                    arg->tm.tm_mday, &mlb_time);
	if (mlb > 0) {
		if (msg.length)
			sc_cb_concat(&msg, " Also, the ");
		else
			sc_cb_concat(&msg, "The ");
		sc_cb_printf(&msg, "%s have a home game at %s.", MLB_TEAM,
		             mlb_time);
		free(mlb_time);
	}
	if (msg.length)
		cbot_send(arg->bot, arg->channel, "%s", msg.buf);
	else if (arg->report_empty)
		cbot_send(arg->bot, arg->channel,
		          "No home games for %s or %s on %04d-%02d-%02d",
		          NBA_TEAMNAME, MLB_TEAM, arg->tm.tm_year,
		          arg->tm.tm_mon, arg->tm.tm_mday);
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
	struct arg *a = calloc(1, sizeof(*a));
	a->channel = strdup(CHANNEL);
	a->bot = bot;
	a->report_empty = true;
	a->reschedule = true;
	localtime_r(&ts, &a->tm);
	a->tm.tm_year += 1900;
	a->tm.tm_mon++;
	sc_lwt_create_task(cbot_get_lwt_ctx(plugin->bot), run_thread, a);
	plugin->data =
	        cbot_schedule_callback(plugin, &callback, NULL, next_run());
}

static void handler(struct cbot_message_event *evt, void *user)
{
	struct tm tm;
	time_t now = time(NULL);
	localtime_r(&now, &tm);

	char *day = sc_regex_get_capture(evt->message, evt->indices, 1);
	if (!*day || strcmp(day, "today") == 0) {
		// already done above
	} else if (strcmp(day, "tomorrow") == 0) {
		tm.tm_mday++;
		mktime(&tm); /* normalize across week days */
	} else {
		strptime(day, "%Y-%m-%d", &tm);
	}
	tm.tm_year += 1900; // really
	tm.tm_mon++;
	CL_DEBUG("looking up schedules for: %04d-%02d-%02d", tm.tm_year,
	         tm.tm_mon, tm.tm_mday);

	struct arg *arg = calloc(1, sizeof(*arg));
	arg->channel = strdup(evt->channel);
	arg->bot = evt->bot;
	arg->tm = tm;
	arg->report_empty = true;
	arg->reschedule = false;
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
	              "games( (today|tomorrow|\\d\\d\\d\\d-\\d\\d-\\d\\d))?");
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
	        "- This plugin checks for Giants & Warriors home games in SF\n"
	        "  Try 'cbot games today' or 'cbot games' for today.\n"
	        "  Try 'cbot games tomorrow' or 'cbot games YYYY-MM-DD' for "
	        "a specific day.");
}

struct cbot_plugin_ops ops = {
	.description = "sports schedules",
	.load = load,
	.unload = unload,
	.help = help,
};
