#ifndef _HELP_CMD_H
#define _HELP_CMD_H

#include <os.h>

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

enum GAP {
	NO_GAP,
	SMALL_GAP,
	LARGE_GAP,
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
		switch(gap) {
			case SMALL_GAP:
				char_gap = 2;
			break;
			case LARGE_GAP:
				char_gap = 30;
			break;
			case NO_GAP:
			default:
				char_gap = gap;
			break;
		}
		STRNCPY_S(line, other, MAX_PATH);
	}
};

#endif
