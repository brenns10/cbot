/**
 * cbot/json.h: JSON helpers built on NOSJ
 *
 * Quite simply, this allows me to prototype better helper APIs in cbot, where I
 * use NOSJ a lot, before merging them into NOSJ itself. So this is a temporary
 * header and APIs may change.
 */
#ifndef CBOT_JSON_H
#define CBOT_JSON_H

#include <nosj.h>
#include <stdbool.h>
#include <stdint.h>

int je_get_object(struct json_easy *je, uint32_t start, const char *key,
                  uint32_t *out);
int je_get_array(struct json_easy *je, uint32_t start, const char *key,
                 uint32_t *out);
int je_get_uint(struct json_easy *je, uint32_t start, const char *key,
                uint64_t *out);
int je_get_int(struct json_easy *je, uint32_t start, const char *key,
               int64_t *out);
int je_get_bool(struct json_easy *je, uint32_t start, const char *key,
                bool *out);
int je_get_string(struct json_easy *je, uint32_t start, const char *key,
                  char **out);
bool je_string_match(struct json_easy *je, uint32_t start, const char *key,
                     const char *cmp);

/** Macro to for loop over each entry in an array */
#define je_arr_for_each(var, jsonp, start)                                     \
	for (var = (jsonp)->tokens[start].length ? ((start) + 1) : 0;          \
	     var != 0; var = ((jsonp)->tokens)[var].next)

/** Macro to for loop over key/value in an object */
#define je_obj_for_each(key, val, jsonp, start)                                \
	for (key = (jsonp)->tokens[start].length ? ((start) + 1) : 0,          \
	    val = key + 1;                                                     \
	     key != 0; key = ((jsonp)->tokens)[key].next, val = key + 1)

#endif // CBOT_JSON_H
