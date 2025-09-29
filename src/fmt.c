#include <sc-collections.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cbot/cbot.h"

int cbot_format(struct sc_charbuf *buf, const char *fmt,
                cbot_formatter_t formatter, void *user)
{
	const char *c, *d;
	struct sc_charbuf cb;
	int rv, count = 0;
	sc_cb_init(&cb, 64);
	while (*fmt) {
		c = strchr(fmt, '{');
		if (!c) {
			sc_cb_concat(buf, fmt);
			rv = count;
			goto out;
		}
		if (c[1] == '{') {
			sc_cb_append(buf, '{');
			fmt = &c[2];
			continue;
		}
		sc_cb_memcpy(buf, fmt, c - fmt);
		d = strchr(c, '}');
		if (!d) {
			rv = -1;
			goto out;
		}
		sc_cb_clear(&cb);
		sc_cb_memcpy(&cb, c + 1, d - c - 1);
		rv = formatter(buf, cb.buf, user);
		count += 1;
		if (rv < 0)
			goto out;
		fmt = d + 1;
	}
	rv = count;
out:
	sc_cb_destroy(&cb);
	return rv;
}

int cbot_format2(struct sc_charbuf *cb, const char *fmt,
                 const cbot_formatter_ops_t *ops, ...)
{
	const char *c, *d;
	struct sc_charbuf token_buf;
	int rv, count = 0;
	va_list args;

	va_start(args, ops);
	sc_cb_init(&token_buf, 64);

	while (*fmt) {
		c = strchr(fmt, '{');
		if (!c) {
			sc_cb_concat(cb, fmt);
			rv = count;
			goto out;
		}
		sc_cb_memcpy(cb, fmt, c - fmt);
		if (c[1] == '{') {
			sc_cb_append(cb, '{');
			fmt = &c[2];
			continue;
		}
		d = strchr(c, '}');
		if (!d) {
			rv = -1;
			goto out;
		}

		sc_cb_clear(&token_buf);
		sc_cb_memcpy(&token_buf, c + 1, d - c - 1);

		const cbot_formatter_ops_t *op = ops;
		int handled = 0;
		while (op && op->prefix) {
			if (op->formatter == CBOT_FMT_CHAIN_SIGNAL) {
				op = (const cbot_formatter_ops_t *)op->prefix;
				continue;
			}
			size_t prefix_len = strlen(op->prefix);
			if (strncmp(token_buf.buf, op->prefix, prefix_len) ==
			    0) {
				const char *suffix = token_buf.buf + prefix_len;
				va_list args_copy;
				va_copy(args_copy, args);
				int args_consumed =
				        op->formatter(cb, suffix, &args_copy);
				va_end(args_copy);
				if (args_consumed < 0) {
					rv = args_consumed;
					goto out;
				}
				for (int i = 0; i < args_consumed; i++) {
					(void)va_arg(args, void *);
				}
				handled = 1;
				count++;
				break;
			}
			op++;
		}

		if (!handled) {
			rv = -1;
			goto out;
		}

		fmt = d + 1;
	}
	rv = count;
out:
	va_end(args);
	sc_cb_destroy(&token_buf);
	return rv;
}

static int printf_formatter(struct sc_charbuf *cb, const char *suffix,
                            va_list *args)
{
	const char *fmt = suffix - 1;
	sc_cb_vprintf(cb, fmt, *args);
	return 1;
}

static int time_formatter(struct sc_charbuf *cb, const char *suffix,
                          va_list *args)
{
	time_t t = va_arg(*args, time_t);
	struct tm tm_info;
	char time_buf[256];

	if (localtime_r(&t, &tm_info) == NULL)
		return -1;

	if (strftime(time_buf, sizeof(time_buf), suffix, &tm_info) == 0)
		return -1;

	sc_cb_concat(cb, time_buf);
	return 1;
}

static int timeg_formatter(struct sc_charbuf *cb, const char *suffix,
                           va_list *args)
{
	time_t t = va_arg(*args, time_t);
	struct tm tm_info;
	char time_buf[256];

	if (gmtime_r(&t, &tm_info) == NULL)
		return -1;

	if (strftime(time_buf, sizeof(time_buf), suffix, &tm_info) == 0)
		return -1;

	sc_cb_concat(cb, time_buf);
	return 1;
}

static int tm_formatter(struct sc_charbuf *cb, const char *suffix,
                        va_list *args)
{
	struct tm *tm_ptr = va_arg(*args, struct tm *);
	char time_buf[256];

	if (strftime(time_buf, sizeof(time_buf), suffix, tm_ptr) == 0)
		return -1;

	sc_cb_concat(cb, time_buf);
	return 1;
}

const cbot_formatter_ops_t cbot_default_formatters[] = {
	{ "%", printf_formatter },
	{ "timel:", time_formatter },
	{ "timeg:", timeg_formatter },
	{ "tm:", tm_formatter },
	{ NULL, NULL }
};
