/**
 * test_name_plugin.c: Unit tests for the name plugin
 */

#include <string.h>
#include <unity.h>

#include "cbot/cbot.h"
#include "plugintest.h"

extern struct cbot_plugin_ops ops;

struct cbot *bot;
struct cbot_plugin *plugin;

void setUp(void)
{
	// Create a test bot
	bot = PT_bot_create("TestBot");
	TEST_ASSERT_NOT_NULL(bot);

	// Load the name plugin
	plugin = PT_load_plugin(bot, &ops, "name");
	TEST_ASSERT_NOT_NULL(plugin);
}

void tearDown(void)
{
	// Clean up
	if (plugin) {
		PT_unload_plugin(plugin);
	}
	if (bot) {
		PT_bot_destroy(bot);
	}
}

static void test_who_is_testbot(void)
{
	// Inject a message asking who the bot is
	PT_inject_message(bot, "#test", "alice", "who is TestBot?", false,
	                  false);

	// Check that the bot responded
	TEST_ASSERT_EQUAL_INT(1, PT_messages_count(bot));

	struct PT_message *msg = PT_messages_get(bot, 0);
	TEST_ASSERT_NOT_NULL(msg);
	TEST_ASSERT_EQUAL_STRING("#test", msg->dest);
	TEST_ASSERT_NOT_NULL(strstr(msg->msg, "TestBot"));
	TEST_ASSERT_NOT_NULL(strstr(msg->msg, "cbot"));
	TEST_ASSERT_NOT_NULL(strstr(msg->msg, "github.com"));
}

static void test_what_is_cbot(void)
{
	// Inject a message asking what cbot is
	PT_inject_message(bot, "#test", "bob", "what is cbot?", false, false);

	// Check that the bot responded
	TEST_ASSERT_EQUAL_INT(1, PT_messages_count(bot));

	struct PT_message *msg = PT_messages_get(bot, 0);
	TEST_ASSERT_NOT_NULL(msg);
	TEST_ASSERT_EQUAL_STRING("#test", msg->dest);
	TEST_ASSERT_NOT_NULL(strstr(msg->msg, "cbot"));
}

static void test_case_insensitive_who(void)
{
	// Test case insensitivity with "Who"
	PT_inject_message(bot, "#test", "charlie", "Who is TestBot", false,
	                  false);

	TEST_ASSERT_EQUAL_INT(1, PT_messages_count(bot));
}

static void test_wtf_is_cbot(void)
{
	// Test the "wtf" variant
	PT_inject_message(bot, "#test", "dave", "wtf is cbot?", false, false);

	TEST_ASSERT_EQUAL_INT(1, PT_messages_count(bot));
}

static void test_unmatched_message(void)
{
	// Test a message that shouldn't match
	PT_inject_message(bot, "#test", "eve", "hello everyone", false, false);

	// Should not respond
	TEST_ASSERT_EQUAL_INT(0, PT_messages_count(bot));
}

static void test_response_in_dm(void)
{
	// Test that it works in DMs too
	PT_inject_message(bot, "frank", "frank", "who are you, TestBot?", false,
	                  true);

	TEST_ASSERT_EQUAL_INT(1, PT_messages_count(bot));

	struct PT_message *msg = PT_messages_get(bot, 0);
	TEST_ASSERT_NOT_NULL(msg);
	TEST_ASSERT_EQUAL_STRING("frank", msg->dest);
}

static void test_with_apostrophe_variations(void)
{
	// Test "what's cbot"
	PT_inject_message(bot, "#test", "grace", "what's cbot?", false, false);

	TEST_ASSERT_EQUAL_INT(1, PT_messages_count(bot));

	PT_messages_clear(bot);

	// Test "who's TestBot"
	PT_inject_message(bot, "#test", "grace", "who's TestBot?", false,
	                  false);

	TEST_ASSERT_EQUAL_INT(1, PT_messages_count(bot));
}

int main(int argc, char **argv)
{
	UNITY_BEGIN();
	RUN_TEST(test_who_is_testbot);
	RUN_TEST(test_what_is_cbot);
	RUN_TEST(test_case_insensitive_who);
	RUN_TEST(test_wtf_is_cbot);
	RUN_TEST(test_unmatched_message);
	RUN_TEST(test_response_in_dm);
	RUN_TEST(test_with_apostrophe_variations);
	return UNITY_END();
}
