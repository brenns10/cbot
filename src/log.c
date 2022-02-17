#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbot/cbot.h"
#include "cbot_private.h"

static int current_log_level;
static FILE *current_log_file;

void cbot_vlog(int level, const char *format, va_list args)
{
	/* clang-tidy decided args is uninitialized? */
	if (current_log_file && level >= current_log_level)
		vfprintf(current_log_file, format, args); // NOLINT
}

void cbot_log(int level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	cbot_vlog(level, format, args);
	va_end(args);
}

void cbot_set_log_level(int level)
{
	current_log_level = level;
}

int cbot_get_log_level(void)
{
	return current_log_level;
}

void cbot_set_log_file(FILE *f)
{
	current_log_file = f;
}

struct levels {
	char *name;
	int level;
};
struct levels levels[] = {
	{ "DEBUG", DEBUG },
	{ "INFO", INFO },
	{ "WARN", WARN },
	{ "CRiT", CRIT },
};

int cbot_lookup_level(const char *str)
{
	for (int i = 0; i < nelem(levels); i++)
		if (strcmp(str, levels[i].name) == 0)
			return levels[i].level;
	return atoi(str);
}
