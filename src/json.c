#include <stdbool.h>
#include <stdint.h>

#include "cbot/json.h"
#include "nosj.h"

int je_get_object(struct json_easy *je, uint32_t start, const char *key,
                  uint32_t *out)
{
	int err = json_easy_lookup(je, start, key, out);
	if (err != JSON_OK)
		return err;
	if (je->tokens[*out].type != JSON_OBJECT)
		return JSONERR_TYPE;
	return JSON_OK;
}
int je_get_array(struct json_easy *je, uint32_t start, const char *key,
                 uint32_t *out)
{
	int err = json_easy_lookup(je, start, key, out);
	if (err != JSON_OK)
		return err;
	if (je->tokens[*out].type != JSON_ARRAY)
		return JSONERR_TYPE;
	return JSON_OK;
}
int je_get_uint(struct json_easy *je, uint32_t start, const char *key,
                uint64_t *out)
{
	uint32_t index;
	int err = json_easy_lookup(je, start, key, &index);
	if (err != JSON_OK)
		return err;
	return json_easy_number_getuint(je, index, out);
}
int je_get_int(struct json_easy *je, uint32_t start, const char *key,
               int64_t *out)
{
	uint32_t index;
	int err = json_easy_lookup(je, start, key, &index);
	if (err != JSON_OK)
		return err;
	return json_easy_number_getint(je, index, out);
}
int je_get_bool(struct json_easy *je, uint32_t start, const char *key,
                bool *out)
{
	uint32_t index;
	int err = json_easy_lookup(je, start, key, &index);
	if (err != JSON_OK)
		return err;
	*out = je->tokens[index].type == JSON_TRUE;
	return JSON_OK;
}
int je_get_string(struct json_easy *je, uint32_t start, const char *key,
                  char **out)
{
	uint32_t index;
	int err = json_easy_lookup(je, start, key, &index);
	if (err != JSON_OK)
		return err;
	return json_easy_string_get(je, index, out);
}

bool je_string_match(struct json_easy *je, uint32_t start, const char *key,
                     const char *cmp)
{
	uint32_t index;
	int err = json_easy_lookup(je, start, key, &index);
	if (err != JSON_OK)
		return false;
	bool match;
	if (json_easy_string_match(je, index, cmp, &match) != JSON_OK)
		return false;
	return match;
}
