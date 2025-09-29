#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sc-collections.h>
#include <unity.h>

#include "cbot/cbot.h"

struct sc_charbuf cb;

void setUp(void)
{
	sc_cb_init(&cb, 64);
}

void tearDown(void)
{
	sc_cb_destroy(&cb);
}

static void test_printf_formatter(void)
{
	int rv;

	rv = cbot_dfmt(&cb, "Hello {%s}!", "world");
	TEST_ASSERT_EQUAL_INT(1, rv);
	TEST_ASSERT_EQUAL_STRING("Hello world!", cb.buf);

	sc_cb_clear(&cb);
	rv = cbot_dfmt(&cb, "Number: {%d}, Hex: {%x}", 42, 255);
	TEST_ASSERT_EQUAL_INT(2, rv);
	TEST_ASSERT_EQUAL_STRING("Number: 42, Hex: ff", cb.buf);
}

static void test_time_formatter(void)
{
	int rv;
	time_t test_time =
	        1234567890; // Known timestamp: Fri Feb 13 23:31:30 2009 UTC

	rv = cbot_dfmt(&cb, "UTC: {timeg:%Y-%m-%d %H:%M:%S}", test_time);
	TEST_ASSERT_EQUAL_INT(1, rv);
	TEST_ASSERT_EQUAL_STRING("UTC: 2009-02-13 23:31:30", cb.buf);

	sc_cb_clear(&cb);
	rv = cbot_dfmt(&cb, "Date: {timeg:%Y-%m-%d}", test_time);
	TEST_ASSERT_EQUAL_INT(1, rv);
	TEST_ASSERT_EQUAL_STRING("Date: 2009-02-13", cb.buf);
}

static void test_mixed_formatters(void)
{
	int rv;
	time_t test_time = 1234567890;

	rv = cbot_dfmt(&cb,
	               "User {%s} logged in at {timeg:%H:%M:%S} with ID {%d}",
	               "alice", test_time, 12345);
	TEST_ASSERT_EQUAL_INT(3, rv);
	TEST_ASSERT_EQUAL_STRING(
	        "User alice logged in at 23:31:30 with ID 12345", cb.buf);
}

static void test_escaped_braces(void)
{
	int rv;

	rv = cbot_dfmt(&cb, "Literal {{braces}} and {%s}", "formatted");
	TEST_ASSERT_EQUAL_INT(1, rv);
	TEST_ASSERT_EQUAL_STRING("Literal {braces}} and formatted", cb.buf);
}

static void test_no_formatters(void)
{
	int rv;

	rv = cbot_dfmt(&cb, "No formatters here");
	TEST_ASSERT_EQUAL_INT(0, rv);
	TEST_ASSERT_EQUAL_STRING("No formatters here", cb.buf);
}

static void test_unknown_formatter(void)
{
	int rv;

	rv = cbot_dfmt(&cb, "Unknown {unknown:test} formatter");
	TEST_ASSERT_LESS_THAN(0, rv);
}

static void test_unclosed_brace(void)
{
	int rv;

	rv = cbot_dfmt(&cb, "Unclosed {%s brace", "test");
	TEST_ASSERT_LESS_THAN(0, rv);
}

static void test_tm_formatter(void)
{
	int rv;
	struct tm test_tm = { .tm_year = 109, // 2009
		              .tm_mon = 1,    // February (0-based)
		              .tm_mday = 13,
		              .tm_hour = 23,
		              .tm_min = 31,
		              .tm_sec = 30 };

	rv = cbot_dfmt(&cb, "Date: {tm:%Y-%m-%d %H:%M:%S}", &test_tm);
	TEST_ASSERT_EQUAL_INT(1, rv);
	TEST_ASSERT_EQUAL_STRING("Date: 2009-02-13 23:31:30", cb.buf);

	sc_cb_clear(&cb);
	rv = cbot_dfmt(&cb, "Short: {tm:%m/%d}", &test_tm);
	TEST_ASSERT_EQUAL_INT(1, rv);
	TEST_ASSERT_EQUAL_STRING("Short: 02/13", cb.buf);
}

// Custom formatter that consumes two arguments: x and y coordinates
static int coord_formatter(struct sc_charbuf *cb, const char *suffix,
                           va_list *args)
{
	int x = va_arg(*args, int);
	int y = va_arg(*args, int);

	if (strcmp(suffix, "xy") == 0) {
		sc_cb_printf(cb, "(%d,%d)", x, y);
	} else if (strcmp(suffix, "dist") == 0) {
		double dist = sqrt(x * x + y * y);
		sc_cb_printf(cb, "%.2f", dist);
	} else {
		return -1;
	}
	return 2; // consumed 2 arguments
}

// Custom formatter that just adds exclamation marks
static int exclaim_formatter(struct sc_charbuf *cb, const char *suffix,
                             va_list *args)
{
	const char *text = va_arg(*args, const char *);
	int count = atoi(suffix);
	if (count <= 0)
		count = 1;
	if (count > 10)
		count = 10; // sanity limit

	sc_cb_concat(cb, text);
	for (int i = 0; i < count; i++) {
		sc_cb_append(cb, '!');
	}
	return 1;
}

const cbot_formatter_ops_t custom_formatters[] = {
	{ "coord:", coord_formatter },
	{ "yell", exclaim_formatter },
	CBOT_FMT_NEXT(cbot_default_formatters),
};

static void test_custom_formatters(void)
{
	int rv;

	// Test multi-argument formatter
	rv = cbot_format2(&cb, "Point: {coord:xy}", custom_formatters, 10, 20);
	TEST_ASSERT_EQUAL_INT(1, rv);
	TEST_ASSERT_EQUAL_STRING("Point: (10,20)", cb.buf);

	sc_cb_clear(&cb);
	rv = cbot_format2(&cb, "Distance: {coord:dist}", custom_formatters, 3,
	                  4);
	TEST_ASSERT_EQUAL_INT(1, rv);
	TEST_ASSERT_EQUAL_STRING("Distance: 5.00", cb.buf);

	// Test single-argument custom formatter
	sc_cb_clear(&cb);
	rv = cbot_format2(&cb, "Message: {yell3}", custom_formatters, "Hello");
	TEST_ASSERT_EQUAL_INT(1, rv);
	TEST_ASSERT_EQUAL_STRING("Message: Hello!!!", cb.buf);

	// Test chaining to default formatters
	sc_cb_clear(&cb);
	rv = cbot_format2(&cb, "Point {coord:xy} has value {%d}",
	                  custom_formatters, 5, 10, 42);
	TEST_ASSERT_EQUAL_INT(2, rv);
	TEST_ASSERT_EQUAL_STRING("Point (5,10) has value 42", cb.buf);

	// Test using default printf formatter through chain
	sc_cb_clear(&cb);
	rv = cbot_format2(&cb, "Simple: {%s} and {%d}", custom_formatters,
	                  "test", 123);
	TEST_ASSERT_EQUAL_INT(2, rv);
	TEST_ASSERT_EQUAL_STRING("Simple: test and 123", cb.buf);

	// Test using default time formatters through chain
	sc_cb_clear(&cb);
	time_t test_time = 1234567890;
	rv = cbot_format2(&cb, "Time: {timeg:%H:%M:%S}", custom_formatters,
	                  test_time);
	TEST_ASSERT_EQUAL_INT(1, rv);
	TEST_ASSERT_EQUAL_STRING("Time: 23:31:30", cb.buf);
}

int main(int argc, char **argv)
{
	UNITY_BEGIN();
	RUN_TEST(test_printf_formatter);
	RUN_TEST(test_time_formatter);
	RUN_TEST(test_mixed_formatters);
	RUN_TEST(test_escaped_braces);
	RUN_TEST(test_no_formatters);
	RUN_TEST(test_unknown_formatter);
	RUN_TEST(test_unclosed_brace);
	RUN_TEST(test_tm_formatter);
	RUN_TEST(test_custom_formatters);
	return UNITY_END();
}
