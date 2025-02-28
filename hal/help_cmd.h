#ifndef _HELP_CMD_H
#define _HELP_CMD_H

#include <os.h>

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

struct help_cmd {
	char line[MAX_PATH];

	// Default constructor
	help_cmd() {
		memset(line, 0, MAX_PATH);
	}

	// Copy constructor
	help_cmd(const char *other) {
		STRNCPY_S(line, other, MAX_PATH);
	}
};

#endif
