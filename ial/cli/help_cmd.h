#ifndef _HELP_CMD_H
#define _HELP_CMD_H

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#define MAX_LINES 20

typedef struct _help_cmd {
	char help_line[MAX_LINES][MAX_PATH];
	int total_lines;
} help_cmd;

#endif
