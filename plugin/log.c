/**
 * log.c: CBot plugin which logs channel messages
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "sc-collections.h"

#include "cbot/cbot.h"

static void write_string(FILE *f, const char *str)
{
	size_t i;
	fputc('"', f);
	for (i = 0; str[i]; i++) {
		switch (str[i]) {
		case '\n':
			fprintf(f, "\\n");
			break;
		case '"':
			fprintf(f, "\"");
			break;
		default:
			fputc(str[i], f);
		}
	}
	fputc('"', f);
}

/*
 * For every channel message, get the current timestamp, open a file of the
 * form: CHANNEL-YYYY-MM-DD.log In append mode, and write out a single-line JSON
 * object, containing:
 * - timestamp: seconds since the epoch, as a float
 * - username: sender of the message
 * - message: content of message
 */
static void cbot_log(struct cbot_message_event *event, void *user)
{
#define NSEC_PER_SEC 10000000000.0
	struct timespec now;
	struct tm *tm;
	double time_float;
	struct sc_charbuf filename;
	FILE *f;

	/*
	 * First get timestamp.
	 */
	clock_gettime(CLOCK_REALTIME, &now);
	tm = localtime(&now.tv_sec);
	time_float = now.tv_sec + now.tv_nsec / NSEC_PER_SEC;

	/*
	 * Create filename and open it.
	 */
	sc_cb_init(&filename, 40);
	sc_cb_printf(&filename, "%s-%04d-%02d-%02d.log", event->channel,
	             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
	f = fopen(filename.buf, "a");

	/*
	 * Write log line.
	 */
	fprintf(f, "{\"timestamp\": %f, \"username\": ", time_float);
	write_string(f, event->username);
	fprintf(f, ", \"message\": ");
	write_string(f, event->message);
	if (event->is_action)
		fprintf(f, ", action: true");
	fprintf(f, "}\n");

	/*
	 * Cleanup
	 */
	fclose(f);
	sc_cb_destroy(&filename);
}

static int load(struct cbot_plugin *plugin, config_setting_t *conf)
{
	cbot_register(plugin, CBOT_MESSAGE, (cbot_handler_t)cbot_log, NULL,
	              NULL);
	return 0;
}

struct cbot_plugin_ops ops = {
	.load = load,
};
