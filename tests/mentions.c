#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>

#include <unity.h>

#include <nosj.h>

#include "../src/signal/internal.h"
#include "cbot/json.h"

// NB: meson incremental builds don't pick up on changes to the JSON file.
// This generally isn't a big deal, but just know that you may need to touch
// the test C file after updating the JSON.
__asm("json_buf: .incbin \"mentions.json\"");
__asm("json_buf_end: .byte 0x00");
extern char json_buf[];

struct json_easy *je;

void setUp(void)
{
	je = json_easy_new(json_buf);
	json_easy_parse(je);
}

void tearDown(void)
{
	// clean stuff up here
	json_easy_destroy(je);
}

static void do_test_mention_from_json(const char *name)
{
	uint32_t item, mentions;
	char *message, *result;

	TEST_ASSERT(je_get_object(je, 0, name, &item) == JSON_OK);
	TEST_ASSERT(je_get_array(je, item, "mentions", &mentions) == JSON_OK);
	TEST_ASSERT(je_get_string(je, item, "message", &message) == JSON_OK);
	TEST_ASSERT(je_get_string(je, item, "result", &result) == JSON_OK);

	char *output = mention_from_json(message, je, mentions);
	TEST_ASSERT_EQUAL_STRING(result, output);

	free(output);
	free(message);
	free(result);
}

static void test_mention_from_json(void)
{
	do_test_mention_from_json("test_basic");
	do_test_mention_from_json("test_two");
	do_test_mention_from_json("test_replace_text");
}

int main(int argc, char **argv)
{
	UNITY_BEGIN();
	RUN_TEST(test_mention_from_json);
	return UNITY_END();
}
