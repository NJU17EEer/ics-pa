/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
	nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_si(char *args) {
	int read_ch_num;
	/* default excute once */
	int inst_exec_steps = 1;	

	if (args != NULL) {
		read_ch_num = sscanf(args, "%d", &inst_exec_steps);
		if (read_ch_num <= 0) {
			printf("Failed to extract integer value from string [%s]\n", args);
			return 0;
		}
	}

	cpu_exec(inst_exec_steps);
	return 0;
}

static int cmd_info(char *args) {

	if (args == NULL) {
		printf("Sub-command [r] is required\n");
	} else if (strcmp(args, "r") == 0) {
		isa_reg_display();
	} else {
		printf("Unknown sub-command [%s]\n", args);
	}

	return 0;
}

static int cmd_x(char *args) {
	char *parse_str = NULL;
	/* delimeter between tokens should be blank */
	const char *delim = " ";
	char *token = NULL;
	int token_num = 0;
	int bytes_num;
	uint32_t st_addr;

	/* arguments parser */
	if (args != NULL) {
		for (token_num = 0, parse_str = args; ;
					token_num++, parse_str = NULL) {
			
			token = strtok(parse_str, delim);
			if (token == NULL) {
				break;
			}

			switch (token_num) {
				case 0:	/* 1-th arguments is N */
					sscanf(token, "%d", &bytes_num);
					break;
				case 1: /* 2-nd argument is EXPR */
					sscanf(token, "%x", &st_addr);
					break;
				default:
					/* TODO: more arguments support */
			}
		}
	} 

	if ((args == NULL) || (token_num != 2)) {
		/* not 2 sub-commands attached */
		printf("Usage: x N EXPR\n");
	} else {
		printf("N = %d, EXPR = 0x%08x\n", bytes_num, st_addr);
	}


	return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
	{ "si", "Execute N instructions then stop. If N isn't specified after si, N = 1", cmd_si},
	{"info", "r: print the state of registers", cmd_info},
	{"x", "usage: x N EXPR, evaluate the EXPR as the start of memory address and print the sequential N bytes. For simplicity, the EXPR should be exactly a hexadecimal number", cmd_x},

  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
