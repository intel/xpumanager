#ifndef _HELP_CMD_H
#define _HELP_CMD_H

#include <os.h>

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

enum GAP {
	NO_GAP,
	SMALL_GAP = 2,
	LARGE_GAP = 30,
	XLARGE_GAP = 37,
};

struct help_cmd {
	char line[MAX_PATH];
	int char_gap;

	// Default constructor
	help_cmd() {
		memset(line, 0, MAX_PATH);
	}

	// Copy constructor
	help_cmd(GAP gap, const char *other) {
		char_gap = (int) gap;
		STRNCPY_S(line, other, MAX_PATH);
	}
};

#endif
