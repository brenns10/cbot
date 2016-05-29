/***************************************************************************//**

  @file         cbot_cli.c

  @author       Stephen Brennan

  @date         Created Monday, 27 July 2015

  @brief        CBot implementation on CLI!

  @copyright    Copyright (c) 2015, Stephen Brennan.  Released under the Revised
                BSD License.  See LICENSE.txt for details.

*******************************************************************************/

#include <stdio.h>

#include "cbot_private.h"
#include "libstephen/ad.h"
#include "libstephen/str.h"

char *name = "cbot";

static void cbot_cli_send(cbot_t *bot, const char *dest, char *format, ...)
{
  va_list va;
  va_start(va, format);
  printf("[%s]%s: ", dest, name);
  vprintf(format, va);
  putchar('\n');
  va_end(va);
}

static void help(void)
{
  puts("usage: cbot cli [options] plugins");
  puts("options:");
  puts("  --name [name]      set the bot's name");
  puts("  --plugin-dir [dir] set the plugin directory");
  puts("  --help             show this help and exit");
  puts("plugins:");
  puts("  list of names of plugins within plugin-dir (don't include \".so\").");
  exit(EXIT_FAILURE);
}

void run_cbot_cli(int argc, char **argv)
{
  char *line, *plugin_dir="plugin";
  cbot_t *bot;
  smb_ad args;
  arg_data_init(&args);

  process_args(&args, argc, argv);
  if (check_long_flag(&args, "name")) {
    name = get_long_flag_parameter(&args, "name");
  }
  if (check_long_flag(&args, "plugin-dir")) {
    plugin_dir = get_long_flag_parameter(&args, "plugin-dir");
  }
  if (check_long_flag(&args, "help")) {
    help();
  }
  if (!(name && plugin_dir)) {
    help();
  }

  bot = cbot_create("cbot");
  bot->actions.send = cbot_cli_send;
  cbot_load_plugins(bot, plugin_dir, ll_get_iter(args.bare_strings));
  while (!feof(stdin)) {
    printf("> ");
    line = read_line(stdin);
    cbot_handle_channel_message(bot, "stdin", "shell", line);
    smb_free(line);
  }

  arg_data_destroy(&args);
  cbot_delete(bot);
}
