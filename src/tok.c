#include <sc-collections.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "cbot/cbot.h"

static ssize_t token_plain(char *s, ssize_t i)
{
	while (s[i] && s[i] != ' ')
		i++;
	if (s[i]) {
		s[i] = '\0';
		i++;
	}
	return i;
}

static ssize_t token_quote(char *s, ssize_t i)
{
	size_t shift = 0;
	while (s[i]) {
		s[i - shift] = s[i];
		if (s[i] == '"') {
			if (s[i + 1] == '"') {
				i++;
				shift++;
			} else if (s[i + 1] == ' ' || s[i + 1] == '\0') {
				s[i - shift] = '\0';
				return i + 1;
			} else {
				return -1;
			}
		}
		i++;
	}
	return -1;
}

static ssize_t token(char *s, struct sc_array *a, ssize_t i)
{
	while (s[i] == ' ')
		i++;

	if (s[i] == '\0')
		return 0;

	if (s[i] == '"') {
		sc_arr_append(a, char *, &s[i + 1]);
		return token_quote(s, i + 1);
	} else {
		sc_arr_append(a, char *, &s[i]);
		return token_plain(s, i);
	}
}

int cbot_tokenize(const char *msg, struct cbot_tok *result)
{
	ssize_t idx = 0;
	struct sc_array a;
	char *str = strdup(msg);
	sc_arr_init(&a, char *, 32);

	while (idx >= 0 && str[idx]) {
		idx = token(str, &a, idx);
	}

	if (idx < 0) {
		free(str);
		sc_arr_destroy(&a);
		return (int)idx;
	} else {
		result->original = str;
		result->tokens = a.arr;
		result->ntok = a.len;
		return a.len;
	}
}

void cbot_tok_destroy(struct cbot_tok *tokens)
{
	free(tokens->original);
	tokens->original = NULL;
	free(tokens->tokens);
	tokens->original = NULL;
}
