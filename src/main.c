/**
 * main.c: Main CBot entry point
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cbot_private.h"

static void usage(char *name)
{
	printf("usage: %s [backend]\n", name);
	puts("backend choices:");
	puts("  irc - run on irc");
	puts("  cli - run in the shell");
	printf("see `%s [backend] --help` for more info on each backend's "
	       "args\n",
	       name);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	srand(time(NULL));
	if (argc < 2) {
		usage(argv[0]);
	}
	if (strcmp(argv[1], "irc") == 0) {
		run_cbot_irc(argc - 2, argv + 2);
	} else if (strcmp(argv[1], "cli") == 0) {
		run_cbot_cli(argc - 2, argv + 2);
	} else {
		usage(argv[0]);
	}
	return EXIT_SUCCESS;
}
